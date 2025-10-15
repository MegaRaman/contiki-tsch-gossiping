#ifndef GCHMAC_TYPES_H_
#define GCHMAC_TYPES_H_

#include "gchmac-conf.h"
#include "linkaddr.h"

#include <stdint.h>

// TODO: old
typedef struct {
    uint8_t colmap_len;
    uint8_t colmap[GCHMAC_COLMAP_MAX_LEN];
} gchmac_packet_hdr_t;

typedef struct {
    gchmac_packet_hdr_t hdr;
    uint16_t data_len;
    uint8_t data[GCHMAC_DATA_MAX_LEN];
} gchmac_packet_old_t;

// new here
//
typedef struct {
    uint16_t colmap;
    uint16_t timeslot;
} gchmac_heartbeat_t;

typedef struct {
    linkaddr_t addr;
    uint16_t timeslot;
    uint8_t ttl;
} gchmac_nbr_t;

typedef enum {
    HEARTBEAT,
    DATA,
} gchmac_packet_type;

#endif
