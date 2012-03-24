#include "jsonsl.h"
#include <assert.h>

jsonsl_t jsonsl_new(int nlevels)
{
    struct jsonsl_st *jsn = malloc(
            sizeof(*jsn) +
            (nlevels * sizeof(struct jsonsl_nest_st)));

    jsn->levels_max = nlevels;
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
        memset(jsn->nests + ii, 0, sizeof(struct jsonsl_nest_st));
    }
}

void jsonsl_destroy(jsonsl_t jsn)
{
    free(jsn);
}

void
jsonsl_feed(jsonsl_t jsn, const char *bytes, size_t nbytes)
{

#define INVOKE_ERROR(eb) \
        if (jsn->error_callback(jsn, JSONSL_ERROR_##eb, nest, (char*)c)) { \
            goto GT_AGAIN; \
        } \
        return;

#define NEST_DESCEND \
    if (jsn->level == jsn->levels_max) { \
        jsn->error_callback(jsn, JSONSL_ERROR_LEVELS_EXCEEDED, nest, (char*)c); \
        return; \
    } \
    jsn->level++; \
    nest = jsn->nests + jsn->level; \
    nest->serial++; \
    nest->complete = 0; \
    nest->level = jsn->level; \
    nest->pos_begin = jsn->pos; \

#define NEST_ASCEND_NOPOS \
    nest->pos_cur = jsn->pos; \
    jsn->level--; \
    nest = jsn->nests + jsn->level;


#define NEST_ASCEND \
    NEST_ASCEND_NOPOS; \
    nest->pos_cur = jsn->pos;

#define CALLBACK(T, state) \
        if (jsn->call_##T) { \
            jsn->nest_callback(jsn, JSONSL_STATE_##state, nest, c); \
        }

    const char *c = bytes;
    int tryagain = 0;
    struct jsonsl_nest_st *nest = jsn->nests + jsn->level;

    while (nbytes) {

        /* Ignore whatever is next in the escape */
        if (jsn->in_escape) {
            jsn->in_escape = 0;
            goto GT_NEXT;
        }

        GT_AGAIN:
        /* Ignore whitespace */
        if (*c <= 0x20) {
            if (nest->type == JSONSL_T_SPECIAL) {
                /* let it be known that the special type has completed */
                CALLBACK(SPECIAL, END);
                NEST_ASCEND;
                jsn->expecting = ',';
            }
            goto GT_NEXT;
        }

        switch (*c) {

            /* Escape */
        case '\\':
            if (nest->type != JSONSL_T_STRING) {
                INVOKE_ERROR(ESCAPE_OUTSIDE_STRING);
            }

            jsn->in_escape = 1;
            goto GT_NEXT;

            /* Beginning or end of string */
        case '"':
        {
            struct jsonsl_nest_st *nest_last = jsn->nests + (jsn->level -1);;

            nest->pos_cur = jsn->pos;
            jsn->can_insert = 0;

            switch (nest->type) {

                /* the end of a string or hash key */
                case JSONSL_T_STRING:
                    CALLBACK(STRING, END);
                    NEST_ASCEND;
                    goto GT_NEXT;
                case JSONSL_T_HKEY:
                    CALLBACK(HKEY, END);
                    NEST_ASCEND;
                    goto GT_NEXT;

                case JSONSL_T_OBJECT:
                    nest->nelem++;
                    if ( (nest->nelem-1) % 2 ) {
                        /* Odd, this must be a hash value */
                        if (jsn->tok_last != ':') {
                            INVOKE_ERROR(MISSING_TOKEN);
                        }
                        assert(jsn->tok_last == ':');
                        jsn->expecting = ','; /* Can't figure out what to expect next */

                        NEST_DESCEND;
                        nest->type = JSONSL_T_STRING;
                        CALLBACK(STRING, BEGIN);

                    } else {
                        /* hash key */
                        if (jsn->expecting != '"') {
                            INVOKE_ERROR(STRAY_TOKEN);
                        }
                        jsn->tok_last = 0;
                        jsn->expecting = ':';
                        jsn->can_insert = 1;

                        NEST_DESCEND;
                        nest->type = JSONSL_T_HKEY;
                        CALLBACK(HKEY, BEGIN);
                    }
                    goto GT_NEXT;

                case JSONSL_T_LIST:
                    nest->nelem++;
                    NEST_DESCEND;
                    nest->type = JSONSL_T_STRING;
                    jsn->expecting = ',';
                    CALLBACK(STRING, BEGIN);
                    goto GT_NEXT;
                default:
                    INVOKE_ERROR(STRING_OUTSIDE_CONTAINER);
                    break;
                } /* switch(nest->type) */
            break;
        }

            /* return to switch(*c) */
            default:
                break; /* make eclipse happy */
        } /* switch (*c) */


        /* ignore string content */
        if (nest->type == JSONSL_T_STRING || nest->type == JSONSL_T_HKEY) {
            goto GT_NEXT;
        }

        switch (*c) {
        case ':':
        case ',':

            if (*c == ',' && nest->type == JSONSL_T_SPECIAL) {
                /* End of special type */
                CALLBACK(SPECIAL, END);
                NEST_ASCEND;
            } else if (jsn->expecting != *c) {
                INVOKE_ERROR(STRAY_TOKEN);
            }

            if (nest->type == JSONSL_T_OBJECT && *c == ',') {
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
            nest->nelem++;

            NEST_DESCEND(jsn, nest, c);
            /* because the constants match the opening delimiters, we can do this: */
            nest->type = *c;
            nest->pos_begin = jsn->pos;
            nest->nelem = 0;
            jsn->can_insert = 1;
            if (*c == '{') {
                /* If we're a hash, we expect a key first, which is quouted */
                jsn->expecting = '"';
            }
            if (*c == JSONSL_T_OBJECT) {
                CALLBACK(OBJECT, BEGIN);
            } else {
                CALLBACK(LIST, BEGIN);
            }
            goto GT_NEXT;

            /* closing of list or object */
        case '}':
        case ']':
            /* cannot insert after a closing object/list */
            jsn->can_insert = 0;
            jsn->level--;

            if (*c == ']') {
                CALLBACK(LIST, END);
            } else {
                CALLBACK(OBJECT, END);
            }
            nest = jsn->nests + jsn->level;
            nest->pos_cur = jsn->pos;
            goto GT_NEXT;

        default:
            /* special */
            if (nest->type != JSONSL_T_SPECIAL) {
                nest->nelem++;

                NEST_DESCEND;
                nest->type = JSONSL_T_SPECIAL;
                CALLBACK(SPECIAL, BEGIN);
            }
            goto GT_NEXT;
        }

        GT_NEXT:
        c++;
        jsn->pos++;
        nbytes--;
    }
}
