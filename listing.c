#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include <pwd.h>
#include <grp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include "mytar.h"
#include "listing.h"



#define CHUNK_SIZE 512
#define REGFILE_TYPEFLAG1 '0'
#define REGFILE_TYPEFLAG2 '\0'
#define NAME_SIZE 100
#define BASE_8 8
#define OWN_GRP 17
#define REGFILE_TYPEFLAG '0'
#define REGFILE_TYPEFLAG2 '\0'
#define SYMLINK_TYPEFLAG '2'
#define DIR_TYPEFLAG '5'

#ifndef S_IFMT
#define S_IFMT 0170000
#endif

int list_contents(int argc, char *argv[], int v, int s){
    int i;
    char *fname;
    struct header *hd = (struct header*)malloc(sizeof(struct header));
    char *hd_string = (char *)hd;
    char null_block[CHUNK_SIZE] = {'\0'};
    FILE *tarfile;
    int file_specified = 0;

    /*check if malloc worked*/
    if (hd == NULL){
        perror("malloc failed");
        exit(1);
    }

    if (argc > 3){
        file_specified = 1;
    }

    /*open the tar file for reading*/
    if ((tarfile = fopen(argv[2], "rb")) == NULL){
        free(hd);
        perror("fopen failed");
        exit(1);
    }

    while(fread(hd, CHUNK_SIZE, 1, tarfile) != 0){
        /*check if the header is valid*/
        if (check_header(hd, s) == 0){
            fprintf(stderr, "Malformed header found. Bailing.\n");
            fclose(tarfile);
            free(hd);
            return 1;
        }
        /*check if it's the end of the file*/
        if (memcmp(hd_string, null_block, CHUNK_SIZE) == 0) {
            /* Checks if there's 2 null blocks */
            if (fread(hd, CHUNK_SIZE, 1, tarfile) &&
            memcmp(hd_string, null_block, CHUNK_SIZE) == 0) {
                break;
            }
        }
        /*go to the next header*/
        if (*(hd->typeflag) == REGFILE_TYPEFLAG1 || 
        *(hd->typeflag) == REGFILE_TYPEFLAG2)
        {
            goto_next_header(hd, tarfile);
        }
        /*build the filename from the prefix and header*/
        fname = build_fullname(hd->prefix, hd->name);
        
        /*if specific files not listed*/
        if (file_specified == 0){
            /*for v*/
            if (!v){
                printf("%s\n", fname);
            }
            else{
                print_complex_v(hd, fname);
            }
        }
        /*if specific files listed, check shopping list(argv)*/
        else{
            for (i = 0; i < argc ; i++){
                if (strncmp(fname, argv[i], strlen(argv[i])) == 0){
                    if(!v){
                        printf("%s\n", fname);
                    }
                    else{
                        print_complex_v(hd, fname);
                    }
                }
            }
        }
        free(fname);
    }
    free(hd);
    fclose(tarfile);
    return 0;
}

/* move to the next header in the tar file*/
void goto_next_header(struct header *hd, FILE *tarfile)
{
    long size = strtol(hd->size, NULL, BASE_8);
    unsigned char buff[CHUNK_SIZE];
    int counter = size / CHUNK_SIZE;
    /* skip blocks only if size != 0 */
    if (strtol(hd->size, NULL, BASE_8))
    {
        fread(buff, sizeof(char), CHUNK_SIZE, tarfile);
    }
    /*keep reading until the whole file is passed*/
    for (; counter > 0; counter--)
    {
        fread(buff, sizeof(char), CHUNK_SIZE, tarfile);
    }
}

int print_complex_v(struct header *hd, char *fname){
    unsigned int size;
    char *perms;
    char *owner;
    char *mtime;;
    perms = format_permissions(hd->mode, hd->typeflag);
    owner = (char *)calloc(65, sizeof(char));
    strcpy(owner, hd->uname);
    strcat(owner, "/");
    strcat(owner, hd->gname);
    /* owner = get_own_grp(hd); */   
    mtime = get_mtime(hd);
    size = strtol(hd->size, NULL, BASE_8);
    printf("%-10s %-17s %8d %-16s %s\n", perms, owner, size, mtime, fname);
    free(owner);
    free(mtime);
    free(perms);
    return 0;
}

char* format_permissions(char* hd_mode, char *typeflag){
    char *formatted;
    int mode = strtol(hd_mode, NULL, BASE_8);
    formatted = (char *) malloc(11);

    if (formatted == NULL){
        return NULL;
    }
    
    switch (*(typeflag)) {
        case DIR_TYPEFLAG:
            formatted[0] = 'd';
            break;
        case SYMLINK_TYPEFLAG:
            formatted[0] = 'l';
            break;
        default:
            formatted[0] = '-';
            break;
    }
    
    /*user permissions*/
    formatted[1] = (mode & S_IRUSR) ? 'r' : '-';
    formatted[2] = (mode & S_IWUSR) ? 'w' : '-';
    formatted[3] = (mode & S_IXUSR) ? 'x' : '-';
    
    /*group permissions*/
    formatted[4] = (mode & S_IRGRP) ? 'r' : '-';
    formatted[5] = (mode & S_IWGRP) ? 'w' : '-';
    formatted[6] = (mode & S_IXGRP) ? 'x' : '-';
    
    /*other permissions*/
    formatted[7] = (mode & S_IROTH) ? 'r' : '-';
    formatted[8] = (mode & S_IWOTH) ? 'w' : '-';
    formatted[9] = (mode & S_IXOTH) ? 'x' : '-';
    
    /*add null terminator*/
    formatted[10] = '\0';  
    
    return formatted;
}

char *get_mtime(struct header *hd){
    char *mtime = (char *)calloc(17, sizeof(char));
    /* Get the mtime*/
    time_t time = strtoll(hd->mtime, NULL, BASE_8);
    if(mtime == NULL)
    {
        perror("calloc failed");
        exit(1);
    }
    /*format time as per requirement*/
    strftime(mtime, 17, "%Y-%m-%d %H:%M", localtime(&time));
    return mtime;
}


char *build_fullname(char *prefix, char *name){
    char *fname;

    if (strlen(prefix) > 0){
        fname = (char *)malloc(strlen(prefix)+ NAME_SIZE + 2);
        if (fname == NULL){
            perror("malloc failed");
            exit(1);
        }
        strcpy(fname, prefix);
        strcat(fname, "/");
        strncat(fname, name, NAME_SIZE);
    }
    else{
        fname = (char *)malloc((size_t)(NAME_SIZE + 1));
        if (fname == NULL){
            perror("malloc failed");
            exit(1);
        }
        strncpy(fname, name, NAME_SIZE);
    }
    return fname;
}