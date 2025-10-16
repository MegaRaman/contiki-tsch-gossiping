#ifndef GCHMAC_PACKET_H_
#define GCHMAC_PACKET_H_

#include "gchmac-types.h"
int parse_gchmac_heartbeat(const uint8_t *data, uint16_t len, gchmac_heartbeat_t *out);

#endif
