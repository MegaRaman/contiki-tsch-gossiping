#ifndef DGMAC_PACKET_H_
#define DGMAC_PACKET_H_

#include "dgmac-types.h"
int parse_dgmac_heartbeat(const uint8_t *data, uint16_t len, dgmac_heartbeat_t *out);

#endif
