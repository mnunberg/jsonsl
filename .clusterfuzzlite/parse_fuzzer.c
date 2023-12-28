#include <stdint.h>
#include <jsonsl.h>


void state_callback(jsonsl_t jsn,
                   jsonsl_action_t action,
                   struct jsonsl_state_st *state,
                   const char *buf)
{

}


int error_callback(jsonsl_t jsn,
                   jsonsl_error_t err,
                   struct jsonsl_state_st *state,
                   char *errat)
{
    return 0;
}


int 
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    jsonsl_t jsn;

    jsn = jsonsl_new(0x100);
    jsn->error_callback = error_callback;
    jsn->action_callback = state_callback;

    jsonsl_feed(jsn, (const char*)data, size);
    jsonsl_destroy(jsn);

    return 0;
}