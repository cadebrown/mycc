/* extras/cat.c - 'cat' unix utility, written in this subset of C
 *
 *
 * to run:
 *  ./mycc README.md src/lex.l < extras/cat.c
 *
 * @author: Cade Brown <me@cade.site>
 */

/* opaque structure; only use 'FILE*' */
typedef void FILE;

/* C std */
extern int perror(char* msg);
extern FILE* fopen(char* src, char* mode);
extern int fread(void* data, size_t sz, size_t n, FILE* fp);
extern int fclose(FILE* fp);

int main(int argc, char** argv) {
    int i;
    for (i = 1; i < argc; i += 1) {
        FILE* fp = fopen(argv[i], "r");
        if (!fp) return perror("failed to open file");
        char tmp;

        /* exhaust file 1 byte at a time */
        while (fread(&tmp, 1, 1, fp) == 1) printf("%c", tmp);

        fclose(fp);
    }
    return 0;
}
