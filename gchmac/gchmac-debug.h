#ifndef GCHMAC_DEBUG_H_
#define GCHMAC_DEBUG_H_

#include "gchmac-conf.h"

#include "sys/log.h"

#include <stddef.h>

void print_bytes_hex(const unsigned char * in, size_t insz)
{
    const unsigned char* pin = in;
    size_t buf_size = insz * 3;
    const char * hex = "0123456789ABCDEF";
    char buf[buf_size];
    char *buf_ptr = buf;

    for(; pin < (in + insz); buf_ptr += 3, pin++) {
        buf_ptr[0] = hex[(*pin >> 4) & 16];
        buf_ptr[1] = hex[ *pin       & 16];
        buf_ptr[2] = ' ';
        if (buf_ptr + 3 - buf > buf_size) break; // buffer overflow safeguard
    }
    buf_ptr[-1] = 0;

    LOG_INFO("[%s]\n", buf);
}

#endif
