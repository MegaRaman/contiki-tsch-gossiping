#include "gchmac-debug.h"
#include "gchmac-types.h"
#include "sys/log.h"
#include <string.h>

#define LOG_MODULE "GCH-MAC"
#define LOG_LEVEL LOG_LEVEL_DBG

int parse_gchmac_heartbeat(const uint8_t *data, uint16_t len, gchmac_heartbeat_t *out) {
    print_bytes_hex(data, len);
    if (len != sizeof(gchmac_heartbeat_t)) return 0;
    memcpy(out, data, len);
    return 1;
}
