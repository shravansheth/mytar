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
#include "extracting.h"

#define NAME_SIZE 100
#define LINKNAME_SIZE 100
#define MODE_SIZE 8
#define UID_SIZE 8
#define GID_SIZE 8
#define CHKSUM_SIZE 8
#define CHKSUM_OFF_SIZE 148
#define DEVMAJOR_SIZE 8
#define DEVMINOR_SIZE 8
#define OCTAL_SIZE 8
#define SIZE_SIZE 12
#define MTIME_SIZE 12
#define TYPEFLAG_SIZE 1
#define MAGIC_SIZE 6
#define VERSION_SIZE 2
#define UNAME_SIZE 32
#define GNAME_SIZE 32
#define PREFIX_SIZE 155
#define PADDING_SIZE 12
#define HDR_SIZE 512
#define REGFILE_TYPEFLAG '0'
#define REGFILE_TYPEFLAG2 '\0'
#define SYMLINK_TYPEFLAG '2'
#define DIR_TYPEFLAG '5'
#define MAGIC "ustar\0"
#define VERSION "00"
#define BUFFSIZE 4096
#define BASE_8 8
#define SIZE0 "00000000000"

#define min(a, b) ((a) < (b) ? (a) : (b))

#ifndef UID_MAX
    #define UID_MAX 2147483647U
#endif

#ifndef GID_MAX
    #define GID_MAX 2147483647U
#endif

#define LINK_MAX 100


int extract_all (int argc, char *argv[], int v, int s){
    int i;
    char *fname;
    struct header *hd = (struct header*)malloc(sizeof(struct header));
    char *hd_string = (char *)hd;
    char null_block[HDR_SIZE] = {'\0'};
    FILE *tarfile;
    int move_fptr = 1;
    int file_specified = 0;

    /*chekc if header malloc worked*/
    if (hd == NULL){
        perror("malloc fialed");
        exit(1);
    }

    if (argc > 3){
        file_specified = 1;
    }

    /*open tarfile for reading*/
    if ((tarfile = fopen(argv[2], "rb")) == NULL){
        free(hd);
        perror("fopen failed");
        exit(1);
    }

    /*read the header*/
    while (fread(hd, HDR_SIZE, 1, tarfile)){
        if(check_header(hd, s) == 0){
            fclose(tarfile);
            free(hd);
            return 1;
        }
        /*check if it's the end of the file*/
        if (memcmp(hd_string, null_block, HDR_SIZE) == 0) {
            /* Checks if there's 2 null blocks */
            if (fread(hd, HDR_SIZE, 1, tarfile) &&
            memcmp(hd_string, null_block, HDR_SIZE) == 0) {
                break;
            }
        }
        /*build the file name from prefix and name fields*/
        fname = build_fullname(hd->prefix, hd->name);

        if (!file_specified){
            if(v){
                printf("%s\n", fname);
            }
            unarchive_all(hd, tarfile, fname, v);
        }
        else{
            /*interate through all args, and see if name matches 
            with what was given*/
            for (i = 3; i < argc; i++){
                if (strstr(fname, argv[i]) ||
                (*(hd->typeflag) == DIR_TYPEFLAG && 
                !strncmp(fname, argv[i], 
                min(strlen(fname), strlen(argv[i])))))
                {
                    if (v){
                        printf("%s\n", fname);
                    }
                    else{
                        /*the file pointer doesn't need to be moved anymore*/
                        if (unarchive_all(hd, tarfile, fname, v) == 0){
                            move_fptr = 0;
                        }
                    }
            }
            }
            if (move_fptr){
                goto_next_header(hd, tarfile);
            }
            move_fptr = 1;
        }
        free(fname);
    }
    free(hd);
    fclose(tarfile);
    return 0;
}

int unarchive_all(struct header *hd, FILE *tarfile, char *fname, int v){
    /*check the typeflags*/
    if (*(hd->typeflag) == DIR_TYPEFLAG)
    {
        /*make the directory*/
        if (mkdir(fname, 0777) == -1){
            if (chmod(fname, 0777) == -1){
                free(hd);
                perror("chmod failed");
                exit(1);
            }
            return 0;
        }
        else{
            return 0;
        }
    }
    else if (*(hd->typeflag) == SYMLINK_TYPEFLAG){
        /*make the symlink*/
        if (symlink(hd->linkname, fname) == -1){
            free(hd);
            perror("symlink failed");
            exit(1);
        }
        return 0;
    }
    else if ((*(hd->typeflag) == REGFILE_TYPEFLAG) ||
    (*(hd->typeflag) == REGFILE_TYPEFLAG2)) {
        unarchive_file(hd, tarfile, fname);
        return 0;
    }
    else{
        printf("Wtf is this type?! Skipping\n");
        return 1;
    }
}

int unarchive_file(struct header *hd, FILE *tarfile, char *fname){
    FILE *unarchived;
    long size = strtol(hd->size, NULL, BASE_8);
    struct utimbuf *utimebuf;
    long bytes_read = 0;
    long bytes_to_read;
    long bytes_remaining;
    char buffer[HDR_SIZE];
    long total_read = 0;
    long offset;
    long mode;


    /*allocate memory for utime bufferstruct */
    utimebuf = (struct utimbuf*) calloc(1, sizeof(struct utimbuf));
    if (utimebuf == NULL){
        perror("calloc failed");
        exit(1);
    }

    restore_folders(fname);

    /*open the file for writing*/
    unarchived = fopen(fname, "wb");
    if(unarchived == NULL){
        perror("fopen failed");
        free(utimebuf);
        exit(1);
    }

    while (total_read < size){
        /*keep tract of how many bytes are written and how many more we 
        need to write*/
        bytes_remaining = size - total_read;
        bytes_to_read = bytes_remaining < HDR_SIZE ? bytes_remaining : HDR_SIZE;

        bytes_read = fread(buffer, sizeof(char), bytes_to_read, tarfile);
        total_read += bytes_read;

        /*write those many bytes*/
        fwrite(buffer, sizeof(char), bytes_read, unarchived);

        /*reached eof before expected*/
        if (bytes_read < bytes_to_read){
            break;
        }
    }
    /*calculate the offset to the next header*/
    offset = (HDR_SIZE - bytes_read);
    /*move the file pointer to the next header*/
    if (fseek(tarfile, offset, SEEK_CUR) != 0){
        perror("fseek failed");
        free(utimebuf);
        exit(1);
    }

    /*change permissions and owner information*/
    if (chmod(fname, 0666) == -1){
        perror("chmod failed");
        free(utimebuf);
        exit(1);
    }

    mode = strtol(hd->mode, NULL, BASE_8);
    if(mode & S_IXUSR || mode & S_IXGRP || mode & S_IXOTH){
        if (chmod(fname, 0777) == -1){
            perror("chmod failed");
            free(utimebuf);
            free(utimebuf);
            exit(1);
        }
    }

    /*restore the old mtime*/
    utimebuf->modtime = strtol(hd->mtime, NULL, BASE_8);
    if (utime(fname, utimebuf) == -1){
        perror("utime");
        free(utimebuf);
        exit(1);
    }
    free(utimebuf);
    fclose(unarchived);
    return 0;
}

/*make a new directory*/
void create_dir(char* path) {
    if (mkdir(path, 0777) == -1) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
}

/*restore folder heirarchy while unarchiving*/
void restore_folders(char* path){
    char* dir_path = strdup(path);
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (access(dir_path, F_OK) != 0) {
            create_dir(dir_path);
        }
    }
    free(dir_path);
}