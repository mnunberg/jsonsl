/**
 * Simple SAX/PUSH oriented JSON parser.
 * It does not try to to decode anything for you, but merely tells you when
 * changes in nesting have occurred.
 * It does not consume any memory.
 * It does not support comments.
 */

#ifndef JSONSL_H_
#define JSONSL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

struct jsonsl_st;
typedef struct jsonsl_st *jsonsl_t;

#define JSONSL_MAX_LEVELS 512

typedef enum {
    JSONSL_T_STRING = '"',
    JSONSL_T_OBJECT = '{',
    JSONSL_T_LIST = '[',
    JSONSL_T_SPECIAL = '^',
    JSONSL_T_HKEY = '#',
    JSONSL_T_UNKNOWN = '?'
} jsonsl_type_t;

typedef enum {
    JSONSL_STATE_BEGIN = '(',
    JSONSL_STATE_END = ')',
    JSONSL_STATE_NOSTATE = '$',
    JSONSL_STATE_ERROR = '!',
} jsonsl_state_t;

typedef enum {
    /* Trailing garbage characters */
    JSONSL_ERROR_GARBAGE_TRAILING,

    /* Found a stray token */
    JSONSL_ERROR_STRAY_TOKEN,

    /* We were expecting a token before this one */
    JSONSL_ERROR_MISSING_TOKEN,

    /* Cannot insert because the container is not ready */
    JSONSL_ERROR_CANT_INSERT,

    /* Found a '\' outside a string */
    JSONSL_ERROR_ESCAPE_OUTSIDE_STRING,

    /* Found a ':' outside of a hash */
    JSONSL_ERROR_KEY_OUTSIDE_OBJECT,

    /* found a string outside of a container */
    JSONSL_ERROR_STRING_OUTSIDE_CONTAINER,

    /* Found a null byte in middle of string */
    JSONSL_ERROR_FOUND_NULL_BYTE,

    /* Too many levels */
    JSONSL_ERROR_LEVELS_EXCEEDED
} jsonsl_error_t;


/* Structure for which to represent nesting */
struct jsonsl_nest_st {
    /* offset into the stream from which the nesting ocurred */
    size_t pos_begin;

    /* offset into the stream at which return has controlled back
     * to this nesting
     */
    size_t pos_cur;

    /* type of nesting */
    jsonsl_type_t type : 8;

    /* level of recursion into nesting */
    unsigned int level;

    /* this counter is incremented whenever an update to this
     * particular nesting is made
     */
    unsigned long serial;

    /* how many elements in the object/list.
     * For objects (hashes), an element is either
     * a key or a value. Thus for one complete pair,
     * nelem will be 2.
     * This field has no meaning for string/hkey/special types
     */
    unsigned int nelem;

    /* if this nesting is complete. If true, it means the
     * length of the elements is pos_cur - pos_begin
     */
    int complete;

    /* put anything you want here */
    void *data;
};

/**
 * This is called whenever a new nesting, element, or whatever
 * has started or completed
 */
typedef void (*jsonsl_nest_callback)(
        jsonsl_t,
        jsonsl_state_t,
        struct jsonsl_nest_st*,
        const char *);

/**
 * This is called when an error is encountered.
 * Sometimes it's possible to 'erase' characters (by replacing them
 * with whitespace). If you think you have corrected the error, you
 * can return a true value, in which case the parser will backtrack
 * and try again.
 */
typedef int (*jsonsl_error_callback)(
        jsonsl_t,
        jsonsl_error_t error,
        struct jsonsl_nest_st *,
        char *errat);

struct jsonsl_st {
    /* Public, read-only */
    int level;
    size_t pos;

    /* Public, writeable */
    jsonsl_nest_callback nest_callback;
    jsonsl_error_callback error_callback;

    /* these are boolean flags you can modify. You will be called
     * about notification for each of these types if the corresponding
     * variable is true.
     */
    int call_SPECIAL;
    int call_OBJECT;
    int call_LIST;
    int call_STRING;
    int call_HKEY;

    /* do whatever you want with this */
    void *data;

    /* Private */
    int in_escape;
    char expecting;
    char tok_last;
    int can_insert;
    unsigned int levels_max;

    /* should be last */
    struct jsonsl_nest_st nests[];
};

jsonsl_t jsonsl_new(int nlevels);
void jsonsl_feed(jsonsl_t jsn, const char *bytes, size_t nbytes);
void jsonsl_reset(jsonsl_t jsn);
void jsonsl_destroy(jsonsl_t jsn);



#endif /* JSONSL_H_ */
