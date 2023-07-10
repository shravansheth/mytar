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
#include "mytar.h"
#include "writing.h"
#include <arpa/inet.h>
#include <sys/types.h>


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
#define SYMLINK_TYPEFLAG '2'
#define DIR_TYPEFLAG '5'
#define MAGIC "ustar\0"
#define VERSION "00"
#define BUFFSIZE 4096
#define BASE_8 8
#define SIZE0 "00000000000"

#ifndef UID_MAX
    #define UID_MAX 2147483647U
#endif

#ifndef GID_MAX
    #define GID_MAX 2147483647U
#endif

#define LINK_MAX 100



struct header *make_header(char *path, int v, int s){
    int i;
    struct header *hd;
    struct stat *st;
    unsigned int chksum = 0;
    struct passwd *pw;
    struct group *grp;

    hd = (struct header*)malloc(sizeof(struct header));
    st = (struct stat*)malloc(sizeof(struct stat));
    memset(hd, 0, sizeof(struct header));

    if (lstat(path, st) == -1){
        perror("lstat failed");
        free(hd);
        free(st);
        return NULL;
    }

    /*add name field*/
    /*if the name of the file fits in the name field, put it there and call 
    it a day*/
    if (strlen(path) < NAME_SIZE){
        strcpy(hd->name, path);
        hd->name[strlen(path)] = '\0';
    }
    else if(strlen(path) == NAME_SIZE){
        strncpy(hd->name, path, NAME_SIZE);
    }
    /*sigh.. it doesn't fit, use prefix to store the longass name*/
    else{
        name_helper(hd, path);
    }
    
    /*add the mode field*/
    /*use a mask and store just the 9 permission bits and 3 file type bits*/
    sprintf(hd->mode, "%07o", st->st_mode & 07777);

    /*add the uid field*/
    /*check if the uid fits in 3 octals*/
    if (st->st_uid > 07777777){
        /*if S option is picked, just return 1 and don't archive this 
        file/directory/whatever*/
        if (s == 1){
            free(hd);
            free(st);
            return NULL;
        }
        else{
            /*if S option is not picked, just shove the uid as 
            as binary number*/
            insert_special_int(hd->uid, BASE_8, st->st_uid);
        }
    }
    /*if the uid fits in 3 octal digits*/
    else{
        sprintf(hd->uid, "%07o", st->st_uid);
    }
    
    /*add the gid field*/
    /*if the gid doesn't fit in 3 octal digits, use the magic
    insert function*/
    if (st->st_gid > 07777777){
        insert_special_int(hd->gid, BASE_8, st->st_gid);
    }
    /*just regular copy using sprintf*/
    else{
        sprintf(hd->gid, "%07o", st->st_gid);
    }
    
    /*add the size field*/
    /*if it's a regular file*/
    if (S_ISREG(st->st_mode)){
        /*if it doesn't fit in 4 octals, use the magic write function*/
        if (st->st_size > 077777777777){
            insert_special_int(hd->size, SIZE_SIZE, st->st_size);
        }
        /*just sprintf if it fits*/
        else{
            sprintf(hd->size, "%011lo", (long)st->st_size);
        }
    }
    /*if not file, meaning directory or link or otehr, put 0 for size*/
    else{
        strcpy(hd->size, SIZE0);
    }
    /*add the mtime field*/
    /*check if it fits in 4 octal digits*/
    if (st->st_mtime > 077777777777){
        /*use the magic insert fucntion*/
        insert_special_int(hd->mtime, MTIME_SIZE, st->st_mtime);
    }
    /*just sprintf if it does*/
    else{
        sprintf(hd->mtime, "%011lo", st->st_mtime);
    }

    /*add typeflag field and linkname(if applicable)*/
    /*check if it's a director*/
    if (S_ISDIR(st->st_mode)){
        *(hd->typeflag) = DIR_TYPEFLAG;
    }
    /*check if it's a link*/
    else if (S_ISLNK(st->st_mode)){
        /*if link, copy the link to linkname field*/
        *(hd->typeflag) = SYMLINK_TYPEFLAG;
        readlink(path, hd->linkname, LINK_MAX);
    }
    /*add file typeflag otherwise*/
    else{
        *(hd->typeflag) = REGFILE_TYPEFLAG;
    }
    /*add the magic field*/
    strcpy(hd->magic, MAGIC);
    /*add the version field*/
    strncpy(hd->version, VERSION, VERSION_SIZE);
    /*add the uname field*/
    pw = getpwuid(st->st_uid);
    strcpy(hd->uname, pw->pw_name);
    /*add the gname field*/
    grp = getgrgid(st->st_gid);
    strcpy(hd->gname, grp->gr_name);

    /*calculate and add the chksum field*/
    /*initialize with all spaces*/
    memset(hd->chksum, ' ', CHKSUM_SIZE);
	for (i = 0; i < HDR_SIZE; i++) {
		chksum += (unsigned char)((unsigned char*)hd)[i];
	}
	sprintf(hd->chksum, "%07o", chksum);

    /*initialize the padding to all \0 bytes*/
    memset(hd->padding, '\0', PADDING_SIZE);

    /*free the stat*/
    free(st);
    return hd;
}

/* archiving  directories */
void dir_archive(char *path, FILE *fd_out, int v, int s){
    char *file;
    struct stat *st;
    struct stat *dir_st;
    struct dirent *ent;
    DIR *dir;
    size_t path_len;
    size_t name_len;
    size_t total_len;
    st = (struct stat*)malloc(sizeof(struct stat));
    dir_st = (struct stat*)malloc(sizeof(struct stat));

    /* If path can't be statted, skip it*/
    if (stat(path, dir_st) == -1)
    {
        printf("%s\n", path);
        free(st);
        free(dir_st);
        perror("stat failed");
        return;
    }

    /* If directory can't be opened, skip it*/
    if ((dir = opendir(path)) == NULL)
    {
        free(st);
        free(dir_st);
        perror("opendir failed");
        return;
    }

    /* Go through subdirectories until there are none*/
    while ((ent = readdir(dir)))
    {
        if (ent->d_ino != dir_st->st_ino)
        {
            /*skip cwd and parent directory*/
            if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
            {
                path_len = strlen(path);
                name_len = strlen(ent->d_name);
                total_len = path_len + name_len + 2;
                /*allocate space for the path with the file name*/
                file = malloc(total_len);
                if (file == NULL) {
                    free(st);
                    free(dir_st);
                    perror("malloc");
                    exit(1);
                }

                /*make new path by adding / and the name*/
                strcpy(file, path);
                strcat(file, "/");
                strcat(file, ent->d_name);

                /* If file can't be statted, skip it */
                if (lstat(file, st) == -1)
                {
                    perror("lstat");
                    continue;
                }
                if (v)
                {
                    printf("%s\n", file);
                }
                /* Recursively archive paths inside directory, func 
                takes care of whether it is a file/dir/link */
                archive_path(file, st, fd_out, v, s);
                free(file);
            }
        }
    }
    free(st);
    free(dir_st);
    closedir(dir);
    return;
}

/* Archives a path, looks into whether it's a file/link/dir*/
void archive_path(char *path, struct stat *st, FILE *fd_out, int v, int s)
{
    struct header *hd;
    long fsize;
    char *dirslashadd = (char *) malloc(PATH_MAX);
    /* If it's a reguler file, create a header and write it*/
    if (S_ISREG(st->st_mode))
    {
        hd = make_header(path, v, s);
        /* Write header and body only if header was created. 
         * Otherwise, move on to next file. */
        if (hd != NULL)
        {
            /*write the header*/
            fwrite(hd, HDR_SIZE, 1, fd_out);
            /*make sure that there's stuff to write and file size is not 0*/
            fsize = strtol(hd->size, NULL, BASE_8);
            /*write the content of the file if size > 0*/
            if (fsize != 0){
                write_tarfile(path, fd_out);
            }
            free(hd);
            free(dirslashadd);
            return;
        }
        /*if header making failed, print message and dip*/
        else{
            free(dirslashadd);
            skip_message(path);
            free(hd);
            return;
        }
    }
    /* For symbolic links, make and write header*/
    else if (S_ISLNK(st->st_mode))
    {
        hd = make_header(path, v, s);
        if (hd != NULL)
        {
            free(dirslashadd);
            fwrite(hd, HDR_SIZE, 1, fd_out);
            free(hd);
            return;
        }
        /*if header making failed, print message and dip*/
        else{
            free(dirslashadd);
            skip_message(path);
            free(hd);
            return;
        }
    }
    /* If it's a directory, create a header and traverse through its
     * subdirectories */
    else if (S_ISDIR(st->st_mode))
    {
        /*add trailing / because its a directory*/
        strcpy(dirslashadd, path);
        strcat(dirslashadd, "/");
        hd = make_header(dirslashadd, v, s);
        if (hd != NULL)
        {
            fwrite(hd, HDR_SIZE, 1, fd_out);
            dir_archive(path, fd_out, v, s);
            free(hd);
            free(dirslashadd);
            return;
        }
        /*if header making failed, print message and dip*/
        else{
            free(dirslashadd);
            skip_message(path);
            free(hd);
            return;
        }
    }
}

/*function that writes the file contents to the tar file*/
void write_tarfile(char *path, FILE *fd_out)
{   
    char buff[HDR_SIZE] = {0};
    FILE *file;
    
    file = fopen(path, "rb");
    if (file == NULL)
    {
        perror("fopen failed");
        return;
    }
    /*read from input file into buffer until eof*/
    while (fread(buff, HDR_SIZE, 1, file) == 1)
    {
        /*write in blocks*/
        fwrite(buff, HDR_SIZE, 1, fd_out);
        /*set the block to all zeros*/
        memset(buff, 0, HDR_SIZE);
    }
    /*write final block*/
    fwrite(buff, HDR_SIZE, 1, fd_out);
    /*check if fread failed*/
    if (ferror(file))
    {
        perror("fread failed");
        exit(-1);
    }
    fclose(file);
}

/*finds the last slash, copies everything before that to the prefix
and everything after that to the name field. adds null terminator if it fits
in both*/
void name_helper(struct header *hd, char *path){
    size_t prefix_length;
    char* last_slash = strrchr(path, '/');
    prefix_length = last_slash - path;
    strncpy(hd->prefix, path, prefix_length);
    if (prefix_length < PREFIX_SIZE){
        hd->prefix[prefix_length] = '\0';
    }
    strncpy(hd->name, last_slash + 1, NAME_SIZE - 1);
    if (strlen(hd->name) < NAME_SIZE){
        hd->name[NAME_SIZE - 1] = '\0';
    }
}


/* Throwing an error for when the 'S' flag is used and the uid is too long. */
void skip_message(char *path){
    path[strlen(path)-1] = '\0';
	printf("(insert_octal:255) octal value too long. (015311104)\n"
            "octal value too long. (015311104)\n"
            "./test: Unable to create conforming header.  Skipping.");
    strcat(path, "/");
}



/*Functions provided by the prof*/
uint32_t extract_special_int(char *where, int len) {
/* For interoperability with GNU tar. GNU seems to
* set the high–order bit of the first byte, then
* treat the rest of the field as a binary integer
* in network byte order.
* I don’t know for sure if it’s a 32 or 64–bit int, but for * this 
version, we’ll only support 32. (well, 31)
* returns the integer on success, –1 on failure.
* In spite of the name of htonl(), it converts int32 t */
int32_t val= -1;
if ( (len >= sizeof(val)) && (where[0] & 0x80)) {
/* the top bit is set and we have space * extract the last four bytes */
val = *(int32_t *)(where+len-sizeof(val));
val = ntohl(val); /* convert to host byte order */ }
return val; 
}

int insert_special_int(char *where, size_t size, int32_t val) { 
/* For interoperability with GNU tar. GNU seems to
* set the high–order bit of the first byte, then
* treat the rest of the field as a binary integer
* in network byte order.
* Insert the given integer into the given field
* using this technique. Returns 0 on success, nonzero * otherwise
*/
int err=0;
if(val<0||(size<sizeof(val)) ){
/* if it’s negative, bit 31 is set and we can’t use the flag
* if len is too small, we can’t write it. * done.
*/
err++;
} else {
/* game on....*/
memset(where, 0, size);
*(int32_t *)(where+size-sizeof(val)) = 
htonl(val); /* place the int */ *where |= 0x80; /* set that high–order bit */
}
return err;
}