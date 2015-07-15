#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <jsonsl.h>

int main(int argc, char **argv)
{
    struct stat sb;
    char *buf;
    FILE *fh;
    jsonsl_t jsn;
    int rv, itermax, ii;
    time_t begin_time;
    size_t total_size;
    unsigned long duration;

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

    fh = fopen(argv[1], "rb");
    if (fh == NULL) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }
    buf = malloc(sb.st_size);
    fread(buf, 1, sb.st_size, fh);
    begin_time = time(NULL);

    jsn = jsonsl_new(128);
    for (ii = 0; ii < itermax; ii++) {
        jsonsl_reset(jsn);
        jsonsl_feed(jsn, buf, sb.st_size);
    }

    total_size = sb.st_size * itermax;
    total_size /= (1024*1024);
    duration = time(NULL) - begin_time;
    if (!duration) {
        duration = 1;
    }
    fprintf(stderr, "SPEED: %lu MB/sec\n", total_size/duration);

    jsonsl_dump_global_metrics();
    return 0;
}
