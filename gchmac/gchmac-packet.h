#ifndef GCHMAC_PACKET_H_
#define GCHMAC_PACKET_H_

#include "gchmac-conf.h"
#include "gchmac-debug.h"
#include "gchmac-types.h"

#include "sys/log.h"

#include <stdint.h>
#include <string.h>

int parse_gchmac_packet(const uint8_t* bytes, uint16_t len, gchmac_packet_t* packet) {
    print_bytes_hex(bytes, len);
    unsigned int cursor = 0;

    uint8_t colmap_len = bytes[cursor];
    if (colmap_len > GCHMAC_COLMAP_MAX_LEN) {
        LOG_ERR("parse_gchmac_packet: Collision map length exceeds maximum\n");
        return 0;
    }
    cursor++;

    const uint8_t *colmap_ptr = bytes + cursor;
    cursor += colmap_len;

    uint16_t data_len = 0;
    memcpy(&data_len, bytes + cursor, 2);
    if (data_len > GCHMAC_DATA_MAX_LEN) {
        LOG_ERR("parse_gchmac_packet: Data length exceeds maximum\n");
        return 0;
    }
    cursor += sizeof(uint16_t);

    const uint8_t *data_ptr = bytes + cursor;
    cursor += data_len;
    if (cursor != len) {
        LOG_ERR("parse_gchmac_packet: expected %u bytes, found %u\n", cursor, len);
        return 0;
    };

    packet->hdr.colmap_len = colmap_len;
    memcpy(packet->hdr.colmap, colmap_ptr, colmap_len);
    packet->data_len = data_len;
    memcpy(packet->data, data_ptr, data_len);

    return 1;
}

int create_gchmac_packet(uint8_t* buf, uint16_t* len, gchmac_packet_t* packet) {

    if (packet->hdr.colmap_len > GCHMAC_COLMAP_MAX_LEN) {
        LOG_ERR("create_gchmac_packet: Collision map length exceeds maximum\n");
        return 0;
    }
    if (packet->data_len > GCHMAC_DATA_MAX_LEN) {
        LOG_ERR("create_gchmac_packet: Data length exceeds maximum\n");
        return 0;
    }

    unsigned int cursor = 0;

    memcpy(buf + cursor, &packet->hdr.colmap_len, 1);
    cursor++;

    memcpy(buf + cursor, packet->hdr.colmap, packet->hdr.colmap_len);
    cursor += packet->hdr.colmap_len;

    memcpy(buf + cursor, &packet->data_len, sizeof(uint16_t));
    cursor += sizeof(uint16_t);

    memcpy(buf + cursor, packet->data, packet->data_len);
    cursor += packet->data_len;

    *len = cursor;

    return 1;
}

#endif
