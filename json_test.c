#include "jsonsl.h"
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>


void fmt_level(const char *buf, size_t nbuf, int levels)
{
    char *c = buf;
    int ii;
    for (ii = 0; ii < levels; ii++) {
        putchar('\t');
    }

    while (nbuf && *c) {
        putchar(*c);
        if (*c == '\n') {
            for (ii = 0; ii < levels; ii++) {
                putchar(' ');
            }
        }
        c++;
        nbuf--;
    }
    putchar('\n');
}

void nest_callback(jsonsl_t jsn,
                   jsonsl_state_t state,
                   struct jsonsl_nest_st *nest,
                   const char *buf)
{
    /* We are called here with the jsn object, the state (BEGIN or END),
     * the 'nest' object, which contains information about the level of
     * nesting we are descending into/ascending from, and a pointer to the
     * start position of the detectin of this nesting
     */

    printf("at byte %d: Got nest callback, nesting=%d state=%c, T='%c'\n",
            jsn->pos, nest->level, state, nest->type);
    if (state == JSONSL_STATE_END) {
        size_t nest_len = nest->pos_cur - nest->pos_begin;

        char *buf_begin = buf - nest_len;
        printf("Item closed, %d bytes long\n", nest_len);
        fmt_level(buf_begin, nest_len, nest->level);
    }
}

int error_callback(jsonsl_t jsn,
                   jsonsl_error_t err,
                   struct jsonsl_nest_st *nest,
                   char *errat)
{
    /* Error callback. In theory, this can return a true value
     * and maybe 'correct' and seek ahead of the buffer, and try to
     * do some correction.
     */
    printf("Got parse error at '%c'\n", *errat);
    printf("Error is %d\n", err);
    printf("Remaining text: %s\n", errat);
    abort();
    return 0;
}

int main(void)
{
    char buf[8092];
    size_t nread = 0;
    int fd;
    jsonsl_t jsn;

    fd = open("txt", O_RDONLY);
    assert(fd >= 0);

    /* Declare that we will support up to 512 nesting levels.
     * Each level of nesting requires about ~40 bytes (allocated at initialization)
     * to maintain state information.
     */
    jsn = jsonsl_new(512);

    /* Set up our error callbacks (to be called when an error occurs)
     * and a nest callback (when a level changes in 'nesting')
     */
    jsn->error_callback = error_callback;
    jsn->nest_callback = nest_callback;

    /* Declare that we're intertested in receiving callbacks about
     * json 'Object' and 'List' types.
     */

    jsn->call_OBJECT = 1;
    jsn->call_LIST = 1;

    /* read into the buffer */

    /**
     * To avoid recomputing offsets and relative positioning,
     * we will maintain the buffer, but this is not strictly required.
     */
    nread = read(fd, buf, 8092);
    jsonsl_feed(jsn, buf, nread);
    jsonsl_destroy(jsn);
    return 0;
}
