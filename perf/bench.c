#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <jsonsl.h>

int main(int argc, char **argv)
{
    struct stat sb;
    char *buf;
    FILE *fh;
    jsonsl_t jsn;
    int rv, itermax, ii;
    if (argc < 3) {
        fprintf(stderr, "%s: FILE ITERATIONS\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    sscanf(argv[2], "%d", &itermax);
    rv = stat(argv[1], &sb);
    if (rv != 0) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }

    fh = fopen(argv[1], "r");
    if (fh == NULL) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }
    buf = malloc(sb.st_size);
    fread(buf, 1, sb.st_size, fh);
    jsn = jsonsl_new(512);
    for (ii = 0; ii < itermax; ii++) {
        jsonsl_reset(jsn);
        jsonsl_feed(jsn, buf, sb.st_size);
    }
    jsonsl_dump_global_metrics();
    return 0;
}
