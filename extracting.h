#include <stdio.h>
#include <stdlib.h>


/*function prototypes*/

int extract_all (int argc, char *argv[], int v, int s);
int unarchive_all(struct header *hd, FILE *tarfile, char *fname, int v);
int unarchive_file(struct header *hd, FILE *tarfile, char *fname);
int create_dirs(char* path);
void create_dir(char* path);
void restore_folders(char* path);