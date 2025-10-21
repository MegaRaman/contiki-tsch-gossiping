#ifndef DGMAC_TYPES_H_
#define DGMAC_TYPES_H_

#include "dgmac-conf.h"
#include "linkaddr.h"

#include <stdint.h>

typedef uint16_t colmap_t;

typedef struct {
    colmap_t nbrmap;
    colmap_t colmap;
    uint16_t timeslot;
} dgmac_heartbeat_t;

typedef struct {
    uint8_t ttl;
    uint16_t timeslot;
    linkaddr_t addr;
} dgmac_nbr_t;

typedef enum {
    DGMAC_PACKET_TYPE_HEARTBEAT,
    DGMAC_PACKET_TYPE_DATA,
} dgmac_packet_type;

#define DGMAC_HEARTBEAT_PACKET_LEN (sizeof(uint8_t) + sizeof(dgmac_heartbeat_t))

typedef void (* dgmac_input_callback_t)(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest);

#endif
