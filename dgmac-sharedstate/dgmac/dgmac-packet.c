#include "dgmac-debug.h"
#include "dgmac-types.h"
#include "sys/log.h"
#include <string.h>

#define LOG_MODULE "DG-MAC"
#define LOG_LEVEL LOG_LEVEL_DBG

int parse_dgmac_heartbeat(const uint8_t *data, uint16_t len, dgmac_heartbeat_t *out) {
    print_bytes_hex(data, len);
    if (len != sizeof(dgmac_heartbeat_t)) return 0;
    memcpy(out, data, len);
    return 1;
}
