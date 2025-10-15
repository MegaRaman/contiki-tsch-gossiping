/*
 * Based on the nullnet-brodcast example from contiki-ng, license follows:
 *
 * > Copyright (c) 2017, RISE SICS.
 * > All rights reserved.
 * >
 * > Redistribution and use in source and binary forms, with or without
 * > modification, are permitted provided that the following conditions
 * > are met:
 * > 1. Redistributions of source code must retain the above copyright
 * >    notice, this list of conditions and the following disclaimer.
 * > 2. Redistributions in binary form must reproduce the above copyright
 * >    notice, this list of conditions and the following disclaimer in the
 * >    documentation and/or other materials provided with the distribution.
 * > 3. Neither the name of the Institute nor the names of its contributors
 * >    may be used to endorse or promote products derived from this software
 * >    without specific prior written permission.
 * >
 * > THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * > ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * > IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * > ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * > FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * > DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * > OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * > HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * > LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * > OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * > SUCH DAMAGE.
 * >
 * > This file is part of the Contiki operating system.
 */

#include "gchmac-conf.h"
#include "gchmac-types.h"
#include "gchmac-packet.h"
#include "gchmac-debug.h"

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "process.h"
#include "sys/node-id.h"
#include <string.h>

/* Configuration */
#define SEND_INTERVAL (8 * CLOCK_SECOND)

#if COOJA == 1
static linkaddr_t coordinator_addr =  {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
#endif

static void
initialize_tsch_schedule()
{
  struct tsch_slotframe *sf_common = tsch_schedule_add_slotframe(1, GCHMAC_SLOTFRAME_TIMESLOTS);
  uint16_t slot_offset = 0;

  tsch_schedule_add_link(sf_common,
      LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED,
      LINK_TYPE_ADVERTISING, &tsch_broadcast_address,
      slot_offset, 0, 1);

  slot_offset = (random_rand() % GCHMAC_MAX_NODES) + 1;

  tsch_schedule_add_link(sf_common, LINK_OPTION_TX, LINK_TYPE_NORMAL, &tsch_broadcast_address, slot_offset, 0, 1);
}

/*---------------------------------------------------------------------------*/
PROCESS(nullnet_example_process, "NullNet broadcast example");
AUTOSTART_PROCESSES(&nullnet_example_process);

int
gchmac_callback_packet_ready(void)
{
    #if TSCH_WITH_LINK_SELECTOR
    packetbuf_set_attr(PACKETBUF_ATTR_TSCH_SLOTFRAME, 0xffff);
    packetbuf_set_attr(PACKETBUF_ATTR_TSCH_TIMESLOT, 0xffff);
    packetbuf_set_attr(PACKETBUF_ATTR_TSCH_CHANNEL_OFFSET, 0xffff);
    #endif
    return 0;
}

/*---------------------------------------------------------------------------*/
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  static gchmac_packet_t input_packet;
  if (parse_gchmac_packet((const uint8_t*)data, len, &input_packet) == 0) return;
  if(input_packet.data_len == sizeof(unsigned)) {
    unsigned count;
    memcpy(&count, &input_packet.data, sizeof(count));
    LOG_INFO("Received %u from ", count);
    LOG_INFO_LLADDR(src);
    LOG_INFO_("\n");
  }
}

void input_callback_scan(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest) {

}


// static int gchmac_status;
// enum {
//     GCHMAC_STATUS_SCANNING = 0,
//     // ...
// };

// PROCESS_THREAD(gchmac_scheduler_proc, ev, data) {
//     PROCESS_BEGIN();
//     gchmac_status = GCHMAC_STATUS_SCANNING;

//     static struct etimer scan_timer;

//     nullnet_set_input_callback(input_callback_scan);

//     etimer_set(&scan_timer, GCHMAC_HEARTBEAT_INTERVAL * 2);
//     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&scan_timer));

//     // TODO: build schedule

//     nullnet_set_input_callback(input_callback);

//     // TODO: intermittently broadcast heartbeat in shared slot.



//     PROCESS_END();
// }

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(nullnet_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static unsigned count = 0;
  static gchmac_packet_t out_packet;
  static uint8_t packet_buf[sizeof(gchmac_packet_t)];

  PROCESS_BEGIN();

  initialize_tsch_schedule();

// Cooja motes use link addresses to distinguish nodes, but Sky motes use the node_id global. Hence the following
#if COOJA == 1
  bool is_coordinator = linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr);
#else
  bool is_coordinator = node_id == 1;
#endif

  tsch_set_coordinator(is_coordinator);
  if (is_coordinator) {
      LOG_INFO("Current node is TSCH coordinator\n");
  }
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
