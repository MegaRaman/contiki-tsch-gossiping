#include "dgmac-debug.h"
#include "dgmac-conf.h"
#include "sys/log.h"

#define LOG_MODULE "DG-MAC"
#define LOG_LEVEL LOG_LEVEL_DBG

void print_bytes_hex(const unsigned char * in, size_t insz)
{
    const unsigned char *pin = in;
    const char * hex = "0123456789ABCDEF";
    char out[insz*3];
    char * pout = out;
    for(; pin < in+insz; pout +=3, pin++){
        pout[0] = hex[(*pin>>4) & 0xF];
        pout[1] = hex[ *pin     & 0xF];
        pout[2] = ':';
        if (pout + 3 - out > insz*3){
            /* Better to truncate output string than overflow buffer */
            /* it would be still better to either return a status */
            /* or ensure the target buffer is large enough and it never happen */
            break;
        }
    }
    pout[-1] = 0;

    LOG_INFO("[%s]\n", out);
}
