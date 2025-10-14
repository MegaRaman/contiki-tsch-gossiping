/*
 * Copyright (c) 2017, RISE SICS.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         NullNet broadcast example
 * \author
*         Simon Duquennoy <simon.duquennoy@ri.se>
 *
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include <string.h>
#include <stdio.h> /* For printf() */

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO
/* Configuration */
#define SEND_INTERVAL (8 * CLOCK_SECOND)

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
static linkaddr_t coordinator_addr =  {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
#endif /* MAC_CONF_WITH_TSCH */


#define APP_SLOTFRAME_TIMESLOTS 16
#define APP_MAX_NODES (APP_SLOTFRAME_TIMESLOTS - 1)
#define GCHMAC_COLMAP_MAX_LEN 4
#define GCHMAC_DATA_MAX_LEN 64

void LOG_INFO_HEX(unsigned char * in, size_t insz)
{
    unsigned char * pin = in;
    const char * hex = "0123456789ABCDEF";
    char out[insz*3];
    char * pout = out;
    for(; pin < in+insz; pout +=3, pin++){
        pout[0] = hex[(*pin>>4) & 0xF];
        pout[1] = hex[ *pin     & 0xF];
        pout[2] = ':';
        if (pout + 3 - out > insz*3){
            /* Better to truncate output string than overflow buffer */
            /* it would be still better to either return a status */
            /* or ensure the target buffer is large enough and it never happen */
            break;
        }
    }
    pout[-1] = 0;

    LOG_INFO("[%s]\n", out);
}


typedef struct {
    uint8_t colmap_len;
    uint8_t colmap[GCHMAC_COLMAP_MAX_LEN];
} gchmac_packet_hdr_t;

typedef struct {
    gchmac_packet_hdr_t hdr;
    uint16_t data_len;
    uint8_t data[GCHMAC_DATA_MAX_LEN];
} gchmac_packet_t;

int parse_gchmac_packet(uint8_t* bytes, uint16_t len, gchmac_packet_t* packet) {
    unsigned int cursor = 0;

    uint8_t colmap_len = bytes[cursor];
    if (colmap_len > GCHMAC_COLMAP_MAX_LEN) {
        LOG_ERR("parse_gchmac_packet: Collision map length exceeds maximum\n");
        return 0;
    }
    cursor++;

    uint8_t *colmap_ptr = bytes + cursor;
    cursor += colmap_len;

    uint16_t data_len = 0;
    memcpy(&data_len, bytes + cursor, 2);
    if (data_len > GCHMAC_DATA_MAX_LEN) {
        LOG_ERR("parse_gchmac_packet: Data length exceeds maximum\n");
        return 0;
    }
    cursor += sizeof(uint16_t);

    uint8_t *data_ptr = bytes + cursor;
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

static void
initialize_tsch_schedule()
{
  struct tsch_slotframe *sf_common = tsch_schedule_add_slotframe(1, APP_SLOTFRAME_TIMESLOTS);
  uint16_t slot_offset;

  slot_offset = 0;

  tsch_schedule_add_link(sf_common,
      LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED,
      LINK_TYPE_ADVERTISING, &tsch_broadcast_address,
      slot_offset, 0, 1);

  slot_offset = (random_rand() % APP_MAX_NODES) + 1;

  tsch_schedule_add_link(sf_common, LINK_OPTION_TX, LINK_TYPE_NORMAL, &tsch_broadcast_address, slot_offset, 0, 1);
}

/*---------------------------------------------------------------------------*/
PROCESS(nullnet_example_process, "NullNet broadcast example");
AUTOSTART_PROCESSES(&nullnet_example_process);

/*---------------------------------------------------------------------------*/
void input_callback(void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  static gchmac_packet_t input_packet;
  if (parse_gchmac_packet((uint8_t*)data, len, &input_packet) == 0) return;
  if(input_packet.data_len == sizeof(unsigned)) {
    unsigned count;
    memcpy(&count, &input_packet.data, sizeof(count));
    LOG_INFO("Received %u from ", count);
    LOG_INFO_LLADDR(src);
    LOG_INFO_("\n");
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(nullnet_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static unsigned count = 0;
  static gchmac_packet_t out_packet;
  static uint8_t packet_buf[sizeof(gchmac_packet_t)];

  PROCESS_BEGIN();

  initialize_tsch_schedule();

#if MAC_CONF_WITH_TSCH
  bool is_coordinator = linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr);
  tsch_set_coordinator(is_coordinator);
  if (is_coordinator) {
      LOG_INFO("Current node is TSCH coordinator\n");
  }

#endif /* MAC_CONF_WITH_TSCH */
  out_packet.hdr.colmap_len = 1;
  out_packet.hdr.colmap[0] = 42; // dummy
  /* Initialize NullNet */
  nullnet_buf = &packet_buf[0];
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    LOG_INFO("Sending %u to ", count);
    LOG_INFO_LLADDR(NULL);
    LOG_INFO_("\n");

    out_packet.data_len = sizeof(count);
    *(out_packet.data) = count;
    create_gchmac_packet(packet_buf, &nullnet_len, &out_packet);

    NETSTACK_NETWORK.output(NULL);
    count++;
    etimer_reset(&periodic_timer);
    tsch_schedule_print();
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
