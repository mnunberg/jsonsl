/**
 * JSON Simple/Stacked/Stateful Lexer.
 * - Does not buffer data
 * - Maintains state
 * - Callback oriented
 * - Lightweight and fast. One source file and one header file
 */

#ifndef JSONSL_H_
#define JSONSL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


#ifdef JSONSL_USE_WCHAR
#include <wchar.h>
typedef jsonsl_char_t wchar_t;
#else
typedef char jsonsl_char_t;
#endif /* JSONSL_USE_WCHAR */

#if (!defined(JSONSL_STATE_GENERIC)) && (!defined(JSONSL_STATE_USER_FIELDS))
#warning "JSONSL_STATE_USER_FIELDS not defined. Define this for extra structure fields"
#warning "or define JSONSL_STATE_GENERIC"
#define JSONSL_STATE_GENERIC
#endif /* !defined JSONSL_STATE_GENERIC */

#ifdef JSONSL_STATE_GENERIC
#define JSONSL_STATE_USER_FIELDS
#endif /* JSONSL_STATE_GENERIC */

#define JSONSL_API


#define JSONSL_MAX_LEVELS 512

struct jsonsl_st;
typedef struct jsonsl_st *jsonsl_t;

#define JSONSL_Tf_STRINGY 0x80

#define JSONSL_XTYPE \
    X(STRING, '"'|JSONSL_Tf_STRINGY) \
    X(HKEY, '#'|JSONSL_Tf_STRINGY) \
    X(OBJECT, '{') \
    X(LIST, '[') \
    X(SPECIAL, '^') \
    X(UESCAPE, 'u')

typedef enum {
#define X(o, c) \
    JSONSL_T_##o = c,
    JSONSL_XTYPE
    JSONSL_T_UNKNOWN = '?'
#undef X
} jsonsl_type_t;

#define JSONSL_XACTION \
    X(PUSH, '+') \
    X(POP, '-') \
    X(UESCAPE, 'U') \
    X(ERROR, '!')



typedef enum {
#define X(a,c) \
    JSONSL_ACTION_##a = c,
    JSONSL_XACTION
    JSONSL_ACTION_UNKNOWN = '?'
#undef X
} jsonsl_action_t;

#define JSONSL_XERR \
/* Trailing garbage characters */ \
    X(GARBAGE_TRAILING) \
/* Found a stray token */ \
    X(STRAY_TOKEN) \
/* We were expecting a token before this one */ \
    X(MISSING_TOKEN) \
/* Cannot insert because the container is not ready */ \
    X(CANT_INSERT) \
/* Found a '\' outside a string */ \
    X(ESCAPE_OUTSIDE_STRING) \
/* Found a ':' outside of a hash */ \
    X(KEY_OUTSIDE_OBJECT) \
/* found a string outside of a container */ \
    X(STRING_OUTSIDE_CONTAINER) \
/* Found a null byte in middle of string */ \
    X(FOUND_NULL_BYTE) \
/* Current level exceeds limit specified in constructor */ \
    X(LEVELS_EXCEEDED) \
/* Got a } as a result of an opening [ or vice versa */ \
    X(BRACKET_MISMATCH)

typedef enum {
#define X(e) \
    JSONSL_ERROR_##e,
    JSONSL_XERR
#undef X
    JSONSL_ERROR_GENERIC
} jsonsl_error_t;

/* Structure for which to represent nesting */
struct jsonsl_state_st {
    /**
     * Position offset variables. These are relative to jsn->pos.
     * pos_begin is the position at which this state was first pushed
     * to the stack. pos_cur is the position at which return last controlled
     * to this state (i.e. an immediate child state was popped from it).
     */
    size_t pos_begin;
    size_t pos_cur;

    /* type of nesting */
    jsonsl_type_t type;

    /* level of recursion into nesting */
    unsigned int level;

    /* how many elements in the object/list.
     * For objects (hashes), an element is either
     * a key or a value. Thus for one complete pair,
     * nelem will be 2.
     * This field has no meaning for string/hkey/special types
     */
    unsigned int nelem;

    /*
     * Useful for an opening nest, this will prevent a callback from being
     * invoked on this item or any of its children
     */
    int ignore_callback;

    /* put anything you want here */
#ifndef JSONSL_STATE_GENERIC
    JSONSL_STATE_USER_FIELDS
#else
    void *data;
#endif /* JSONSL_STATE_USER_FIELDS */
};

typedef void (*jsonsl_stack_callback)(
        jsonsl_t,
        jsonsl_action_t,
        struct jsonsl_state_st*,
        const jsonsl_char_t *);

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
        struct jsonsl_state_st *,
        jsonsl_char_t *);

struct jsonsl_st {
    /* Public, read-only */

    /* This is the current level of the stack */
    int level;

    /* This is the current position, relative to the beginning
     * of the stream.
     */
    size_t pos;

    /* This variable is set to the buffer passed to the last
     * call of jsonsl_feed
     */
    const jsonsl_char_t *base;
    /**
     * The following callbacks are invoked when stack changes occur.
     * JSONSL will first check for specific 'POP' and 'PUSH' callbacks,
     * and then check for the generic 'action_callback'.
     *
     * Callbacks for pushes are done _after_ the state has been pushed.
     * This means the level parameters reflect the new state.
     *
     * Callbacks for pops are done _before_ the state is to be popped,
     * Once the callback returns, the level counter is decremented and
     * the state popped.
     */
    jsonsl_stack_callback action_callback;
    jsonsl_stack_callback action_callback_PUSH;
    jsonsl_stack_callback action_callback_POP;

    /**
     * This is the maximum level for which a callback is to be invoked
     */
    unsigned int max_callback_level;
    /**
     * This is an error callback and should be defined. If a parse error happens
     * and you do not have an error handler installed, then your application
     * will crash.
     */
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

    /* This is a special callback, and is invoked as so
     * callback(jsn, JSONSL_ACTION_UESCAPE, state, at);
     * and is called when a '\uf00d' is encountered. *at == 'u'.
     */
    jsonsl_stack_callback action_callback_UESCAPE;
    int call_UESCAPE;
    /* This flag goes in conjunction with call_UESCAPE, and says
     * that we shall return when a '\uf00d' or similar is encountered.
     * At this point, the application may want to gobble/chomp the next
     * four bytes, or store it in its own stack for further processing.
     */
    int return_UESCAPE;

    /* do whatever you want with this */
    void *data;

    /* Private lexer variables. Don't even look at this. */
    int in_escape;
    char expecting;
    char tok_last;
    int can_insert;
    unsigned int levels_max;

    /**
     * This is the stack. Its upper bound is levels_max, or the
     * nlevels argument passed to jsonsl_new. If you modify this structure,
     * make sure that this member is last.
     */
    struct jsonsl_state_st stack[1];
};


/**
 * Creates a new lexer object, with capacity for recursion
 *
 * @param nlevels maximum recursion depth
 */
JSONSL_API
jsonsl_t jsonsl_new(int nlevels);

/**
 * Feeds data into the lexer.
 *
 * @param jsn the lexer object
 * @param bytes new data to be fed
 * @param nbytes size of new data
 */
JSONSL_API
void jsonsl_feed(jsonsl_t jsn, const jsonsl_char_t *bytes, size_t nbytes);

/**
 * Resets the internal parser state. This does not free the parser
 * but does clean it internally, so that the next time feed() is called,
 * it will be treated as a new stream
 *
 * @param jsn the lexer
 */
JSONSL_API
void jsonsl_reset(jsonsl_t jsn);

/**
 * Frees the lexer, cleaning any allocated memory taken
 *
 * @param jsn the lexer
 */
JSONSL_API
void jsonsl_destroy(jsonsl_t jsn);

/**
 * Gets the 'parent' element, given the current one
 *
 * @param jsn the lexer
 * @param cur the current nest, which should be a struct jsonsl_nest_st
 */
JSONSL_API
#define jsonsl_last_state(jsn, cur) \
    (cur->level > 1 ) \
    ? (jsn->stack + (cur->level-1)) \
    : NULL


/**
 * This enables receiving callbacks on all events. Doesn't do
 * anything special but helps avoid some boilerplate.
 * This does not touch the UESCAPE callbacks or flags.
 */
JSONSL_API
#define jsonsl_enable_all_callbacks(jsn) \
    jsn->call_HKEY = 1; \
    jsn->call_STRING = 1; \
    jsn->call_OBJECT = 1; \
    jsn->call_SPECIAL = 1; \
    jsn->call_LIST = 1;

/**
 * These two functions, dump a string representation
 * of the error or type, respectively. They will never
 * return NULL
 */
JSONSL_API
const char* jsonsl_strerror(jsonsl_error_t err);
JSONSL_API
const char* jsonsl_strtype(jsonsl_type_t jt);

#endif /* JSONSL_H_ */
