#include "jsonsl.h"
#include <assert.h>

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

void jsonsl_destroy(jsonsl_t jsn)
{
    free(jsn);
}

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
        } else if (*c > 0x7f) {
            if (state->type & JSONSL_Tf_STRINGY) {
                /* Must be a string. */
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
                state->nelem++;

                STACK_PUSH;
                state->type = JSONSL_T_SPECIAL;
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

const char* jsonsl_strerror(jsonsl_error_t err)
{
#define X(t) \
    if (err == JSONSL_ERROR_##t) \
        return #t;
    JSONSL_XERR;
#undef X
    return "<UNKNOWN_ERROR>";
}

const char *jsonsl_strtype(jsonsl_type_t type)
{
#define X(o,c) \
    if (type == JSONSL_T_##o) \
        return #o;
    JSONSL_XTYPE
#undef X
    return "UNKNOWN TYPE";

}
