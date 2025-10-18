#ifndef GCHMAC_TYPES_H_
#define GCHMAC_TYPES_H_

#include "gchmac-conf.h"
#include "linkaddr.h"

#include <stdint.h>

typedef uint16_t colmap_t;

typedef struct {
    colmap_t nbrmap;
    colmap_t colmap;
    uint16_t timeslot;
} gchmac_heartbeat_t;

typedef struct {
    uint8_t ttl;
    uint16_t timeslot;
    linkaddr_t addr;
} gchmac_nbr_t;

typedef enum {
    GCHMAC_PACKET_TYPE_HEARTBEAT,
    GCHMAC_PACKET_TYPE_DATA,
} gchmac_packet_type;

#define GCHMAC_HEARTBEAT_PACKET_LEN (sizeof(uint8_t) + sizeof(gchmac_heartbeat_t))

typedef void (* gchmac_input_callback_t)(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest);

#endif
