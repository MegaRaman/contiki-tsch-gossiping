#ifndef GCHMAC_TYPES_H_
#define GCHMAC_TYPES_H_

#include "gchmac-conf.h"

#include <stdint.h>

typedef struct {
    uint8_t colmap_len;
    uint8_t colmap[GCHMAC_COLMAP_MAX_LEN];
} gchmac_packet_hdr_t;

typedef struct {
    gchmac_packet_hdr_t hdr;
    uint16_t data_len;
    uint8_t data[GCHMAC_DATA_MAX_LEN];
} gchmac_packet_t;

#endif
