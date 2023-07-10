#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include "mytar.h"
#include "writing.h"
#include "listing.h"
#include "extracting.h"

#define CHKSUM_SIZE 8
#define CHUNK_SIZE 512
#define BASE_8 8
#define SPACE_SIZE 32

int main(int argc, char *argv[]){
    int i;
    int ctx;
    int main_option;
    int v = 0;
    int s = 0;
    int f = 0;
    FILE *fd_out;
    struct stat *st;
    char *path;
    char null_block[CHUNK_SIZE] = {0};

    /*Lot of usage and option handling*/
    if (argc < 2){
        fprintf(stderr,
        "You're missing either the tar filename or the options argument\n");
        return 1;
    }
    ctx = 0;
    for (i=0; i < strlen(argv[1]); i++){
        if (argv[1][i] == 'c'){
            main_option = 'c';
            ctx++;
        }
        else if(argv[1][i] == 't'){
            main_option = 't';
            ctx++;
        }
        else if(argv[1][i] == 'x'){
            main_option = 'x';
            ctx++;
        }
        else if(argv[1][i] == 'v'){
            v = 1;
        }
        else if(argv[1][i] == 'S'){
            s = 1;
        }
        else if(argv[1][i] == 'f'){
            f = 1;
        }
    }
    /*More than one of ctx option picked*/
    if (ctx > 1){
        fprintf(stderr, "You can only pick one of the ctx option\n");
        return 1;
    }
    /*None of the ctx options was picked. BRO PICK ONE*/
    else if (ctx == 0){
        fprintf(stderr, "You need to pick atleast one of ctx option\n");
        return 1;
    }
    /*Tar filename not provided. bro this is not the real tar. this is MYTAR*/
    else if(f == 0){
        fprintf(stderr, "You need to provide a tar filename(f option)\n");
        return 1;
    }

    /*Create option*/
    if (main_option == 'c'){
        
        /*check if Mr. User even gave us something to archive*/
        if (argc < 4){
            fprintf(stderr,
            "You did not provide a path or files to be archived");
            return 1;
        }
        /*open the tar file to write in*/
        fd_out = fopen(argv[2], "wb");
        if (fd_out == NULL){
            perror("fopen");
            exit(-1);
        }
        
        /*allocated memory for status and path*/
        st = (struct stat*)malloc(sizeof(struct stat));
        path = (char *)malloc(PATH_MAX);

        for (i = 3; i < argc; i++){
            /*lstat the ith path provided*/
            if (lstat(argv[i], st) == -1){
                /*do nothing because it could be anything, so we just skip 
                that file/directory or whatever the poop that was*/
            }
            else{
                if (v == 1){
                    if (S_ISDIR(st->st_mode)){
                        printf("%s/\n", argv[i]);
                    }
                    else{
                        printf("%s\n", argv[i]);
                    }
                }
                /*archive the path. A lot of other functions doing the job*/
                archive_path(argv[i], st, fd_out, v, s);
            }
        }
        /*write 2 null blocks at the end of the tar file*/
        fwrite(null_block, CHUNK_SIZE, 1, fd_out);
        fwrite(null_block, CHUNK_SIZE, 1, fd_out);
        fclose(fd_out);
        free(st);
        free(path);
        return 0;
    }
    else if (main_option == 't'){
        if (list_contents(argc, argv, v, s) == 1){
            exit(1);
        }
    }
    else if (main_option == 'x') {
        extract_all(argc, argv, v, s);
    }
    return 0;
}

int check_header(struct header *hd, int s){
    int i;
    unsigned int chksum = 0;
    char null_block[CHUNK_SIZE] = {'\0'};
    unsigned char *hd_string = (unsigned char *)hd;
    /* If it is empty block, header invalid, return 1*/
    if (memcmp((char *)hd_string, null_block, CHUNK_SIZE) == 0)
    {
        return 1;
    }
    /* if s option, check magic and version*/
    if (s)
    {
        
        if (hd->magic[5] != '\0' 
            || hd->version[0] != '0' || hd->version[1] != '0')
        {
            return 0;
        }
    }
    /*check if the chksums match*/
    /*add up all bytes from the header*/
    for (i = 0; i < CHUNK_SIZE; i++) {
	    chksum += (unsigned char)hd_string[i];
	}
    /*subtract the chksum bytes*/
    for (i = 0; i < CHKSUM_SIZE; i++){
        chksum -= hd->chksum[i];
    }
    /*add the spaces size*/
    chksum += CHKSUM_SIZE * SPACE_SIZE;
    /* If checksums arent equal, return 0 */
    if (chksum != (strtol(hd->chksum, NULL, BASE_8)))
    {
        return 0;
    }
    
    return 1;
}