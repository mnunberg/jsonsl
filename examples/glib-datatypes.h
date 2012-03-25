#define _XTYPE_ALL \
    X(TYPE_LIST) \
    X(TYPE_HASH) \
    X(TYPE_BOOLEAN) \
    X(TYPE_INTEGER) \
    X(TYPE_STRING)

typedef enum {
#define X(t) t,
    _XTYPE_ALL
    TYPE_UNKNOWN
#undef X
} jtype_t;


#define ST_ELEMENT_BASE(ptype) \
    jtype_t type; \
    struct element_st *parent; \
    ptype *data;

struct element_st { ST_ELEMENT_BASE(void) };
struct string_st {
    ST_ELEMENT_BASE(const char)
};

struct list_st {
    ST_ELEMENT_BASE(GList)
};

struct hash_st {
    ST_ELEMENT_BASE(GHashTable)
    const char *pending_key;
};

struct objgraph_st {
    struct element_st *root;
};
