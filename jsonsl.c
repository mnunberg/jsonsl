#include "jsonsl.h"
#include <assert.h>
#include <limits.h>
#include <ctype.h>

/* Predeclare the table. The actual table is in the end of this file */
static jsonsl_special_t *Special_table;
#define extract_special(c) \
    Special_table[(unsigned int)(c & 0xff)]

JSONSL_API
jsonsl_t jsonsl_new(int nlevels)
{
    struct jsonsl_st *jsn =
            calloc(1, sizeof (*jsn) +
                    ( (nlevels-1) * sizeof (struct jsonsl_state_st) )
            );

    jsn->levels_max = nlevels;
    jsn->max_callback_level = -1;
    jsonsl_reset(jsn);
    return jsn;
}

JSONSL_API
void jsonsl_reset(jsonsl_t jsn)
{
    int ii;
    jsn->tok_last = 0;
    jsn->can_insert = 1;
    jsn->pos = 0;
    jsn->level = 0;
    jsn->in_escape = 0;
    jsn->expecting = 0;

    for (ii = 0; ii < jsn->levels_max; ii++) {
        memset(jsn->stack + ii, 0, sizeof(struct jsonsl_state_st));
    }
}

JSONSL_API
void jsonsl_destroy(jsonsl_t jsn)
{
    if (jsn) {
        free(jsn);
    }
}

JSONSL_API
void
jsonsl_feed(jsonsl_t jsn, const jsonsl_char_t *bytes, size_t nbytes)
{

#define INVOKE_ERROR(eb) \
    if (jsn->error_callback(jsn, JSONSL_ERROR_##eb, state, (char*)c)) { \
        goto GT_AGAIN; \
    } \
    return;

#define STACK_PUSH \
    if (jsn->level == jsn->levels_max) { \
        jsn->error_callback(jsn, JSONSL_ERROR_LEVELS_EXCEEDED, state, (char*)c); \
        return; \
    } \
    jsn->level++; \
    state = jsn->stack + jsn->level; \
    state->ignore_callback = jsn->stack[jsn->level-1].ignore_callback; \
    state->level = jsn->level; \
    state->pos_begin = jsn->pos;

#define STACK_POP_NOPOS \
    state->pos_cur = jsn->pos; \
    jsn->level--; \
    state = jsn->stack + jsn->level;


#define STACK_POP \
    STACK_POP_NOPOS; \
    state->pos_cur = jsn->pos;

#define SPECIAL_MAYBE_POP \
    if (state->type == JSONSL_T_SPECIAL) { \
        state->pos_cur = jsn->pos; \
        CALLBACK(SPECIAL, POP); \
        STACK_POP; \
        jsn->expecting = 0; \
    }

#define CALLBACK(T, action) \
    if (jsn->call_##T && \
            jsn->max_callback_level > state->level && \
            state->ignore_callback == 0) { \
        \
        if (jsn->action_callback_##action) { \
            jsn->action_callback_##action(jsn, JSONSL_ACTION_##action, state, c); \
        } else if (jsn->action_callback) { \
            jsn->action_callback(jsn, JSONSL_ACTION_##action, state, c); \
        } \
    }

    const jsonsl_char_t *c = bytes;
    struct jsonsl_state_st *state = jsn->stack + jsn->level;
    jsn->base = bytes;

    while (nbytes) {
        /* Special escape handling for some stuff */
        if (jsn->in_escape) {
            jsn->in_escape = 0;
            if (*c == 'u') {
                CALLBACK(UESCAPE, UESCAPE);
                if (jsn->return_UESCAPE) {
                    return;
                }
            }
            goto GT_NEXT;
        }

        GT_AGAIN:
        /* Ignore whitespace */
        if (*c <= 0x20) {
            SPECIAL_MAYBE_POP;
            goto GT_NEXT;
        } else if (*(jsonsl_uchar_t*)c > 0x7f) {
            if (state->type & JSONSL_Tf_STRINGY) {
                state->special_flags = JSONSL_SPECIALf_NONASCII;
                /* non-ascii, not a token. */
                goto GT_NEXT;
            } else {
                INVOKE_ERROR(GARBAGE_TRAILING);
            }
        }

        switch (*c) {

            /* Escape */
        case '\\':
            if (! (state->type == JSONSL_T_STRING || state->type == JSONSL_T_HKEY) ) {
                INVOKE_ERROR(ESCAPE_OUTSIDE_STRING);
            }

            jsn->in_escape = 1;
            goto GT_NEXT;

            /* Beginning or end of string */
        case '"':
            /* Cannot have a numeric/boolean/null after/before a string */
            if (state->type == JSONSL_T_SPECIAL) {
                INVOKE_ERROR(STRAY_TOKEN);
            }
        {
            state->pos_cur = jsn->pos;
            jsn->can_insert = 0;

            switch (state->type) {

                /* the end of a string or hash key */
                case JSONSL_T_STRING:
                    CALLBACK(STRING, POP);
                    STACK_POP;
                    goto GT_NEXT;
                case JSONSL_T_HKEY:
                    CALLBACK(HKEY, POP);
                    STACK_POP;
                    goto GT_NEXT;

                case JSONSL_T_OBJECT:
                    state->nelem++;
                    if ( (state->nelem-1) % 2 ) {
                        /* Odd, this must be a hash value */
                        if (jsn->tok_last != ':') {
                            INVOKE_ERROR(MISSING_TOKEN);
                        }
                        jsn->expecting = ','; /* Can't figure out what to expect next */

                        STACK_PUSH;
                        state->type = JSONSL_T_STRING;
                        CALLBACK(STRING, PUSH);

                    } else {
                        /* hash key */
                        if (jsn->expecting != '"') {
                            INVOKE_ERROR(STRAY_TOKEN);
                        }
                        jsn->tok_last = 0;
                        jsn->expecting = ':';
                        jsn->can_insert = 1;

                        STACK_PUSH;
                        state->type = JSONSL_T_HKEY;
                        CALLBACK(HKEY, PUSH);
                    }
                    goto GT_NEXT;

                case JSONSL_T_LIST:
                    state->nelem++;
                    STACK_PUSH;
                    state->type = JSONSL_T_STRING;
                    jsn->expecting = ',';
                    CALLBACK(STRING, PUSH);
                    goto GT_NEXT;
                default:
                    INVOKE_ERROR(STRING_OUTSIDE_CONTAINER);
                    break;
                } /* switch(state->type) */
            break;
        }

            /* return to switch(*c) */
            default:
                break; /* make eclipse happy */
        } /* switch (*c) */


        /* ignore string content */
        if (state->type & JSONSL_Tf_STRINGY) {
            goto GT_NEXT;
        }

        switch (*c) {
        case ':':
        case ',':

            if (*c == ',') {
                SPECIAL_MAYBE_POP;
                jsn->expecting = ','; /* hack */
            }

            if (jsn->expecting != *c) {
                INVOKE_ERROR(STRAY_TOKEN);
            }

            if (state->type == JSONSL_T_OBJECT && *c == ',') {
                /* end of hash value, expect a string as a hash key */
                jsn->expecting = '"';
            }

            jsn->tok_last = *c;
            jsn->can_insert = 1;
            goto GT_NEXT;

            /* new list or object */
        case '[':
        case '{':
            if (!jsn->can_insert) {
                INVOKE_ERROR(CANT_INSERT);
            }
            state->nelem++;

            STACK_PUSH;
            /* because the constants match the opening delimiters, we can do this: */
            state->type = *c;
            state->pos_begin = jsn->pos;
            state->nelem = 0;
            jsn->can_insert = 1;
            if (*c == '{') {
                /* If we're a hash, we expect a key first, which is quouted */
                jsn->expecting = '"';
            }
            if (*c == JSONSL_T_OBJECT) {
                CALLBACK(OBJECT, PUSH);
            } else {
                CALLBACK(LIST, PUSH);
            }
            jsn->tok_last = 0;
            goto GT_NEXT;

            /* closing of list or object */
        case '}':
        case ']':
            SPECIAL_MAYBE_POP;
            jsn->can_insert = 0;
            jsn->level--;
            jsn->expecting = ',';
            if (*c == ']') {
                if (state->type != '[') {
                    INVOKE_ERROR(BRACKET_MISMATCH);
                }
                CALLBACK(LIST, POP);
            } else {
                if (state->type != '{') {
                    INVOKE_ERROR(BRACKET_MISMATCH);
                }
                CALLBACK(OBJECT, POP);
            }
            state = jsn->stack + jsn->level;
            state->pos_cur = jsn->pos;
            goto GT_NEXT;

        default:
            /* special */
            if (state->type != JSONSL_T_SPECIAL) {
                int special_flags = extract_special(*c);
                if (!special_flags) {
                    INVOKE_ERROR(SPECIAL_EXPECTED);
                }

                state->nelem++;
                STACK_PUSH;
                state->type = JSONSL_T_SPECIAL;
                state->special_flags = special_flags;
                CALLBACK(SPECIAL, PUSH);
            }
            goto GT_NEXT;
        }

        GT_NEXT:
        c++;
        jsn->pos++;
        nbytes--;
    }
}

JSONSL_API
const char* jsonsl_strerror(jsonsl_error_t err)
{
#define X(t) \
    if (err == JSONSL_ERROR_##t) \
        return #t;
    JSONSL_XERR;
#undef X
    return "<UNKNOWN_ERROR>";
}

JSONSL_API
const char *jsonsl_strtype(jsonsl_type_t type)
{
#define X(o,c) \
    if (type == JSONSL_T_##o) \
        return #o;
    JSONSL_XTYPE
#undef X
    return "UNKNOWN TYPE";

}

/*
 *
 * JPR/JSONPointer functions
 *
 *
 */
#ifndef JSONSL_NO_JPR
static
jsonsl_jpr_type_t
populate_component(char *in,
                   struct jsonsl_jpr_component_st *component,
                   char **next,
                   jsonsl_error_t *errp)
{
    unsigned long pctval;
    char *c = NULL, *outp = NULL, *end = NULL;
    size_t input_len;
    jsonsl_jpr_type_t ret = JSONSL_PATH_NONE;

    if (*next == NULL || *(*next) == '\0') {
        return JSONSL_PATH_NONE;
    }

    /* Replace the next / with a NULL */
    *next = strstr(in, "/");
    if (*next != NULL) {
        *(*next) = '\0'; /* drop the forward slash */
        input_len = *next - in;
        end = *next;
        *next += 1; /* next character after the '/' */
    } else {
        input_len = strlen(in);
        end = in + input_len + 1;
    }

    component->pstr = in;

    /* Check for special components of interest */
    if (*in == JSONSL_PATH_WILDCARD_CHAR && input_len == 1) {
        /* Lone wildcard */
        ret = JSONSL_PATH_WILDCARD;
        goto GT_RET;
    } else if (isdigit(*in)) {
        /* ASCII Numeric */
        char *endptr;
        component->idx = strtoul(in, &endptr, 10);
        if (endptr && *endptr == '\0') {
            ret = JSONSL_PATH_NUMERIC;
            goto GT_RET;
        }
    }

    /* Default, it's a string */
    ret = JSONSL_PATH_STRING;
    for (c = outp = in; c < end; c++, outp++) {
        char origc;
        if (*c != '%') {
            goto GT_ASSIGN;
        }
        /*
         * c = { [+0] = '%', [+1] = 'b', [+2] = 'e', [+3] = '\0' }
         */

        /* Need %XX */
        if (c+2 >= end) {
            *errp = JSONSL_ERROR_PERCENT_BADHEX;
            return JSONSL_PATH_INVALID;
        }
        if (! (isxdigit(*(c+1)) && isxdigit(*(c+2))) ) {
            *errp = JSONSL_ERROR_PERCENT_BADHEX;
            return JSONSL_PATH_INVALID;
        }

        /* Temporarily null-terminate the characters */
        origc = *(c+3);
        *(c+3) = '\0';
        pctval = strtoul(c+1, NULL, 16);
        *(c+3) = origc;

        *outp = pctval;
        c += 2;
        continue;

        GT_ASSIGN:
        *outp = *c;
    }
    /* Null-terminate the string */
    for (; outp < c; outp++) {
        *outp = '\0';
    }

    GT_RET:
    component->ptype = ret;
    if (ret != JSONSL_PATH_WILDCARD) {
        component->len = strlen(component->pstr);
    }
    return ret;
}

JSONSL_API
jsonsl_jpr_t
jsonsl_jpr_new(const char *path, jsonsl_error_t *errp)
{
    char *my_copy;
    int count, curidx;
    struct jsonsl_jpr_st *ret;
    struct jsonsl_jpr_component_st *components;
    size_t origlen;
    jsonsl_error_t errstacked;

    if (errp == NULL) {
        errp = &errstacked;
    }

    if (path == NULL || *path != '/') {
        *errp = JSONSL_ERROR_JPR_NOROOT;
        return NULL;
    }

    count = 1;
    path++;
    {
        const char *c = path;
        for (; *c; c++) {
            if (*c == '/') {
                count++;
                if (*(c+1) == '/') {
                    *errp = JSONSL_ERROR_JPR_DUPSLASH;
                    return NULL;
                }
            }
        }
    }
    if(*path) {
        count++;
    }

    components = malloc(sizeof(*components) * count);
    my_copy = malloc(strlen(path) + 1);
    strcpy(my_copy, path);

    components[0].ptype = JSONSL_PATH_ROOT;

    if (*my_copy) {
        char *cur = my_copy;
        int pathret = JSONSL_PATH_STRING;
        curidx = 1;
        while (pathret > 0 && curidx < count) {
            pathret = populate_component(cur, components + curidx, &cur, errp);
            if (pathret > 0) {
                curidx++;
            } else {
                break;
            }
        }

        if (pathret == JSONSL_PATH_INVALID) {
            free(components);
            free(my_copy);
            return NULL;
        }
    } else {
        curidx = 1;
    }


    origlen = strlen(path) + 1;
    ret = malloc(sizeof(*ret));
    ret->components = components;
    ret->ncomponents = curidx;
    ret->basestr = my_copy;
    ret->orig = malloc(origlen);
    strcpy(ret->orig, path);

    return ret;
}

void jsonsl_jpr_destroy(jsonsl_jpr_t jpr)
{
    free(jpr->components);
    free(jpr->basestr);
    free(jpr->orig);
    free(jpr);
}

JSONSL_API
jsonsl_jpr_match_t
jsonsl_jpr_match(jsonsl_jpr_t jpr,
                   jsonsl_type_t parent_type,
                   unsigned int parent_level,
                   const char *key,
                   size_t nkey)
{
    /* find our current component. This is the child level */
    int cmpret;
    struct jsonsl_jpr_component_st *p_component;
    p_component = jpr->components + parent_level;

    if (parent_level >= jpr->ncomponents) {
        return JSONSL_MATCH_NOMATCH;
    }

    /* Lone query for 'root' element. Always matches */
    if (parent_level == 0) {
        if (jpr->ncomponents == 1) {
            return JSONSL_MATCH_COMPLETE;
        } else {
            return JSONSL_MATCH_POSSIBLE;
        }
    }

    /* Wildcard, always matches */
    if (p_component->ptype == JSONSL_PATH_WILDCARD) {
        if (parent_level == jpr->ncomponents-1) {
            return JSONSL_MATCH_COMPLETE;
        } else {
            return JSONSL_MATCH_POSSIBLE;
        }
    }

    /* Check numeric array index */
    if (p_component->ptype == JSONSL_PATH_NUMERIC
            && parent_type == JSONSL_T_LIST) {
        if (p_component->idx != nkey) {
            return JSONSL_MATCH_NOMATCH;
        } else {
            if (parent_level == jpr->ncomponents-1) {
                return JSONSL_MATCH_COMPLETE;
            } else {
                return JSONSL_MATCH_POSSIBLE;
            }
        }
    }

    /* Check lengths */
    if (p_component->len != nkey) {
        return JSONSL_MATCH_NOMATCH;
    }

    /* Check string comparison */
    cmpret = strncmp(p_component->pstr, key, nkey);
    if (cmpret != 0) {
        return JSONSL_MATCH_NOMATCH;
    } else {
        if (parent_level == jpr->ncomponents-1) {
            return JSONSL_MATCH_COMPLETE;
        } else {
            return JSONSL_MATCH_POSSIBLE;
        }
    }

    /* Never reached, but make the compiler happy */
    abort();
    return JSONSL_MATCH_NOMATCH;
}

JSONSL_API
void jsonsl_jpr_match_state_init(jsonsl_t jsn,
                                 jsonsl_jpr_t *jprs,
                                 size_t njprs)
{
    int ii, *firstjmp;
    if (njprs == 0) {
        return;
    }
    jsn->jprs = malloc(sizeof(jsonsl_jpr_t) * njprs);
    jsn->jpr_count = njprs;
    jsn->jpr_root = calloc(1, sizeof(int) * njprs * jsn->levels_max);
    memcpy(jsn->jprs, jprs, sizeof(jsonsl_jpr_t) * njprs);
    /* Set the initial jump table values */

    firstjmp = jsn->jpr_root;
    for (ii = 0; ii < njprs; ii++) {
        firstjmp[ii] = ii+1;
    }
}

JSONSL_API
void jsonsl_jpr_match_state_cleanup(jsonsl_t jsn)
{
    if (jsn->jpr_count == 0) {
        return;
    }

    free(jsn->jpr_root);
    free(jsn->jprs);
    jsn->jprs = NULL;
    jsn->jpr_root = NULL;
    jsn->jpr_count = 0;
}

/**
 * This function should be called exactly once on each element...
 * This should also be called in recursive order, since we rely
 * on the parent having been initalized for a match.
 *
 * Since the parent is checked for a match as well, we maintain a 'serial' counter.
 * Whenever we traverse an element, we expect the serial to be the same as a global
 * integer. If they do not match, we re-initialize the context, and set the serial.
 *
 * This ensures a type of consistency without having a proactive reset by the
 * main lexer itself.
 *
 */
JSONSL_API
jsonsl_jpr_t jsonsl_jpr_match_state(jsonsl_t jsn,
                                    struct jsonsl_state_st *state,
                                    const char *key,
                                    size_t nkey,
                                    jsonsl_jpr_match_t *out)
{
    struct jsonsl_state_st *parent_state;
    jsonsl_jpr_t ret = NULL;

    /* Jump and JPR tables for our own state and the parent state */
    int *jmptable, *pjmptable;
    int jmp_cur, ii, ourjmpidx;

    if (!jsn->jpr_root) {
        *out = JSONSL_MATCH_NOMATCH;
        return NULL;
    }

    pjmptable = jsn->jpr_root + (jsn->jpr_count * (state->level-1));
    jmptable = pjmptable + jsn->jpr_count;

    /* If the parent cannot match, then invalidate it */
    if (*pjmptable == 0) {
        *jmptable = 0;
        *out = JSONSL_MATCH_NOMATCH;
        return NULL;
    }

    parent_state = jsn->stack + state->level - 1;

    if (parent_state->type == JSONSL_T_LIST) {
        nkey = parent_state->nelem;
    }

    *jmptable = 0;
    ourjmpidx = 0;
    memset(jmptable, 0, sizeof(int) * jsn->jpr_count);

    for (ii = 0; ii <  jsn->jpr_count; ii++) {
        jmp_cur = pjmptable[ii];
        if (jmp_cur) {
            jsonsl_jpr_t jpr = jsn->jprs[jmp_cur-1];
            *out = jsonsl_jpr_match(jpr,
                                    parent_state->type,
                                    parent_state->level,
                                    key, nkey);
            if (*out == JSONSL_MATCH_COMPLETE) {
                ret = jpr;
                *jmptable = 0;
                return ret;
            } else if (*out == JSONSL_MATCH_POSSIBLE) {
                jmptable[ourjmpidx] = ii+1;
                ourjmpidx++;
            }
        } else {
            break;
        }
    }
    if (!*jmptable) {
        *out = JSONSL_MATCH_NOMATCH;
    }
    return NULL;
}

JSONSL_API
const char *jsonsl_strmatchtype(jsonsl_jpr_match_t match)
{
#define X(T,v) \
    if ( match == JSONSL_MATCH_##T ) \
        return #T;
    JSONSL_XMATCH
#undef X
    return "<UNKNOWN>";
}

#endif /* JSONSL_WITH_JPR */

/**
 * Character table for special lookups, generated by a little perl script I wrote
 * (can be found in srcutil)
 */
static jsonsl_special_t _special_table[0xff] = {
        /* 0x00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x1f */
        /* 0x20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x2c */
        /* 0x2d */ JSONSL_SPECIALf_SIGNED /* - */, /* 0x2d */
        /* 0x2e */ 0,0, /* 0x2f */
        /* 0x30 */ JSONSL_SPECIALf_UNSIGNED /* 0 */, /* 0x30 */
        /* 0x31 */ JSONSL_SPECIALf_UNSIGNED /* 1 */, /* 0x31 */
        /* 0x32 */ JSONSL_SPECIALf_UNSIGNED /* 2 */, /* 0x32 */
        /* 0x33 */ JSONSL_SPECIALf_UNSIGNED /* 3 */, /* 0x33 */
        /* 0x34 */ JSONSL_SPECIALf_UNSIGNED /* 4 */, /* 0x34 */
        /* 0x35 */ JSONSL_SPECIALf_UNSIGNED /* 5 */, /* 0x35 */
        /* 0x36 */ JSONSL_SPECIALf_UNSIGNED /* 6 */, /* 0x36 */
        /* 0x37 */ JSONSL_SPECIALf_UNSIGNED /* 7 */, /* 0x37 */
        /* 0x38 */ JSONSL_SPECIALf_UNSIGNED /* 8 */, /* 0x38 */
        /* 0x39 */ JSONSL_SPECIALf_UNSIGNED /* 9 */, /* 0x39 */
        /* 0x3a */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x59 */
        /* 0x5a */ 0,0,0,0,0,0,0,0,0,0,0,0, /* 0x65 */
        /* 0x66 */ JSONSL_SPECIALf_FALSE /* f */, /* 0x66 */
        /* 0x67 */ 0,0,0,0,0,0,0, /* 0x6d */
        /* 0x6e */ JSONSL_SPECIALf_NULL /* n */, /* 0x6e */
        /* 0x6f */ 0,0,0,0,0, /* 0x73 */
        /* 0x74 */ JSONSL_SPECIALf_TRUE /* t */, /* 0x74 */
        /* 0x75 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x94 */
        /* 0x95 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xb4 */
        /* 0xb5 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xd4 */
        /* 0xd5 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xf4 */
        /* 0xf5 */ 0,0,0,0,0,0,0,0,0,0 /* 0xfe */
};
static jsonsl_special_t *Special_table = _special_table;
