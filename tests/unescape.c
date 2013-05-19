#include <stdio.h>
#include <stdlib.h>
#include <jsonsl.h>
#include <assert.h>
#include <wchar.h>
#include <locale.h>
#include <string.h>
#include "all-tests.h"

static size_t res;
static jsonsl_error_t err;
static char *out;
static int strtable[0xff] = { 0 };
const char *escaped;

/**
 * Check a single octet escape of four hex digits
 */
void test_single_uescape(void)
{
    escaped = "\\u002B";
    strtable['u'] = 1;
    out = malloc(strlen(escaped)+1);
    res = jsonsl_util_unescape(escaped, out, strlen(escaped), strtable, &err);
    assert(res == 1);
    assert(out[0] == 0x2b);
    free(out);
}

/**
 * Test that we handle the null escape correctly (or that we do it right)
 */
void test_null_escape(void)
{
    escaped = "\\u0000";
    strtable['u'] = 1;
    out = malloc(strlen(escaped)+1);
    res = jsonsl_util_unescape(escaped, out, strlen(escaped), strtable, &err);
    assert(res == 1);
    assert(out[0] == '\0');
    free(out);
}

/**
 * Test multiple sequences of escapes.
 */
void test_multibyte_escape(void)
{
    int mbres;
    jsonsl_special_t flags;
    wchar_t dest[4]; /* שלום */
    escaped = "\\uD7A9\\uD79C\\uD795\\uD79D";
    strtable['u'] = 1;
    out = malloc(strlen(escaped));
    res = jsonsl_util_unescape_ex(escaped,
                                  out,
                                  strlen(escaped),
                                  strtable,
                                  &flags,
                                  &err,
                                  NULL);
    assert(res == 8);
    mbres = mbstowcs(dest, out, 4);
    assert(memcmp(L"שלום", dest,
           8) == 0);
    assert(flags & JSONSL_SPECIALf_NONASCII);
    free(out);
}

/**
 * Check that things we don't want being unescaped are not unescaped
 */
void test_ignore_escape(void)
{
    escaped = "Some \\nWeird String";
    out = malloc(strlen(escaped)+1);
    strtable['W'] = 0;
    res = jsonsl_util_unescape(escaped, out, strlen(escaped), strtable, &err);
    out[res] = '\0';
    assert(res == strlen(escaped));
    assert(strncmp(escaped, out, res) == 0);

    escaped = "\\tA String";
    res = jsonsl_util_unescape(escaped, out, strlen(escaped), strtable, &err);
    out[res] = '\0';
    assert(res == strlen(escaped));
    assert(strncmp(escaped, out, res) == 0);
    free(out);
}

/**
 * Check that the built-in mappings for the 'sane' defaults work
 */
void test_replacement_escape(void)
{
    escaped = "This\\tIs\\tA\\tTab";
    out = malloc(strlen(escaped)+1);
    strtable['t'] = 1;
    res = jsonsl_util_unescape(escaped, out, strlen(escaped), strtable, &err);
    assert(res > 0);
    out[res] = '\0';
    assert(out[4] == '\t');
    assert(strcmp(out, "This\tIs\tA\tTab") == 0);
    free(out);
}

void test_invalid_escape(void)
{
    escaped = "\\invalid \\escape";
    out = malloc(strlen(escaped)+1);
    res = jsonsl_util_unescape(escaped, out, strlen(escaped), strtable, &err);
    assert(res == 0);
    assert(err == JSONSL_ERROR_ESCAPE_INVALID);
    free(out);
}

JSONSL_TEST_UNESCAPE_FUNC
{
    char *curlocale = setlocale(LC_ALL, "");
    test_single_uescape();
    test_null_escape();
    if (curlocale && strstr(curlocale, "UTF-8") != NULL) {
        test_multibyte_escape();
    } else {
        fprintf(stderr,
                "Skipping multibyte tests because LC_ALL is not set to UTF8\n");
    }
    test_ignore_escape();
    test_replacement_escape();
    test_invalid_escape();
    return 0;
}
