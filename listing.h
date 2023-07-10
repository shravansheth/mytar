

/*function prototypes*/

int list_contents(int argc, char *argv[], int v, int s);
int print_complex_v(struct header *hd, char *fname);
char *format_permissions(char* hd_mode, char* typeflag);
char *get_own_grp(struct header *hd);
char *get_mtime(struct header *hd);
char *build_fullname(char *prefix, char *name);
void goto_next_header(struct header *hd, FILE *tarfile);