#include <arpa/inet.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>



/*function prototypes*/

struct header *make_header(char *path, int v, int s);
void l_print(char *path);
void name_helper(struct header *hd, char *path);
void write_tarfile(char *path, FILE *fd_out);
uint32_t extract_special_int(char *where, int len);
int insert_special_int(char *where, size_t size, int32_t val);
void skip_message(char *path);
void archive_path(char *path, struct stat *sb, FILE *fd_out, int v, int s);
void dir_archive(char *path, FILE *fd_out, int v, int s);