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

#include "gchmac.h"
#include "etimer.h"
#include "gchmac-conf.h"
#include "gchmac-debug.h"
#include "gchmac-types.h"
#include "gchmac-packet.h"

#include "linkaddr.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "packetbuf.h"
#include "process.h"

#include "tsch/tsch.h"
#include "watchdog.h"
#include <stdint.h>
#include <string.h>

#include "sys/log.h"
#define LOG_MODULE "GCH-MAC"
#define LOG_LEVEL LOG_LEVEL_DBG

#if COOJA != 1
#include "sys/node-id.h"
#endif

#include <string.h>

/* Configuration */
#if COOJA == 1
static linkaddr_t coordinator_addr =  {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
#endif

static gchmac_nbr_t neighbors[GCHMAC_MAX_NEIGHBORS];

static colmap_t combined_nbrmap = 0;
static colmap_t combined_nbrmap_buf = 0;

static uint16_t tx_timeslot = 0;
static bool detected_collision = false;

static uint16_t rx_timeslot = 0;
static bool rx_slot_configured = false;

static uint8_t heartbeat_buf[GCHMAC_HEARTBEAT_PACKET_LEN] = { 0 };
static uint8_t payload_buf[GCHMAC_PACKET_MAX_LEN] = { 0 };

static gchmac_input_callback_t upper_input_callback = NULL;

static void configure_receive_slot(void);

PROCESS(gchmac_heartbeat_proc, "heartbeat thread");

static void
initialize_tsch_schedule()
{
    LOG_DBG("Initializing tsch schedule\n");
    struct tsch_slotframe *sf_common = tsch_schedule_add_slotframe(1, GCHMAC_SLOTFRAME_TIMESLOTS);
    uint16_t slot_offset = 0;

    tsch_schedule_add_link(sf_common,
        LINK_OPTION_RX | LINK_OPTION_TX | LINK_OPTION_SHARED,
        LINK_TYPE_ADVERTISING, &tsch_broadcast_address,
        slot_offset, 0, 1);
}

/*---------------------------------------------------------------------------*/

int
gchmac_callback_packet_ready(void)
{
    gchmac_packet_type type = *(uint8_t*)packetbuf_dataptr();
    uint16_t timeslot;
    switch(type) {
        case GCHMAC_PACKET_TYPE_DATA:
            timeslot = tx_timeslot;
            break;
        default:
            timeslot = 0;
            break;
    }
    #if TSCH_WITH_LINK_SELECTOR
    packetbuf_set_attr(PACKETBUF_ATTR_TSCH_SLOTFRAME, 1);
    packetbuf_set_attr(PACKETBUF_ATTR_TSCH_TIMESLOT, timeslot);
    packetbuf_set_attr(PACKETBUF_ATTR_TSCH_CHANNEL_OFFSET, 0xffff);
    #endif

    LOG_DBG("Broadcasting on timeslot %u: \n", timeslot);
    print_bytes_hex(packetbuf_dataptr(), packetbuf_datalen());
    return 0;
}

/*---------------------------------------------------------------------------*/

void choose_tx_slot() {
    if (~combined_nbrmap == 1) {
        LOG_ERR("no more free timeslots\n");
    }
    uint16_t new_slot = 1 + random_rand() % GCHMAC_MAX_NEIGHBORS;
    // while(1) {
    //     if (new_slot == tx_timeslot || (combined_nbrmap & (1 << new_slot)) != 0) {
    //         new_slot = 1 + (new_slot + 1) % GCHMAC_MAX_NEIGHBORS;
    //         continue;
    //     }
    //     break;
    // }

    while ( (new_slot == tx_timeslot) ||((combined_nbrmap & (1 << new_slot)) != 0) ) {
        new_slot = 1 + (new_slot + 1) % GCHMAC_MAX_NEIGHBORS;
    }
    struct tsch_slotframe *sl = tsch_schedule_get_slotframe_by_handle(1);
    if (tx_timeslot != 0) {
        tsch_schedule_remove_link_by_offsets(sl, tx_timeslot, 0);
    }
    tx_timeslot = new_slot;
    LOG_DBG("together tx: %u\n", tx_timeslot);
    tsch_schedule_add_link(sl, LINK_OPTION_TX, LINK_TYPE_NORMAL, &tsch_broadcast_address, tx_timeslot, 0, 1);

    // Add TX link
    tsch_schedule_add_link(sl,
        LINK_OPTION_TX,  // TX only
        LINK_TYPE_NORMAL,
        &tsch_broadcast_address,
        tx_timeslot, 0, 1);

    // Also configure receive slot when choosing TX slot
    configure_receive_slot();
}

static void configure_receive_slot(void)
{
    if (rx_slot_configured && rx_timeslot != 0) {
        return; // Already configured
    }

    // Find an available slot for receiving (different from TX slot)
    uint16_t new_rx_slot = 1 + random_rand() % (GCHMAC_SLOTFRAME_TIMESLOTS - 1);

    // Make sure RX slot is different from TX slot
    while (new_rx_slot == tx_timeslot) {
        new_rx_slot = 1 + (new_rx_slot + 1) % (GCHMAC_SLOTFRAME_TIMESLOTS - 1);
    }

    struct tsch_slotframe *sl = tsch_schedule_get_slotframe_by_handle(1);

    // Remove old RX link if it exists
    if (rx_timeslot != 0) {
        tsch_schedule_remove_link_by_offsets(sl, rx_timeslot, 0);
    }

    // Add dedicated RX link
    rx_timeslot = new_rx_slot;
    tsch_schedule_add_link(sl,
        LINK_OPTION_RX,  // RX only - dedicated receive slot
        LINK_TYPE_NORMAL,
        &tsch_broadcast_address,
        rx_timeslot, 0, 1);

    rx_slot_configured = true;
    LOG_DBG("together rx: %u\n", rx_timeslot);
}

void heartbeat_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    static gchmac_heartbeat_t heartbeat;

    if (!parse_gchmac_heartbeat(data, len, &heartbeat)) {
        LOG_ERR("gchmac: Failed to parse heartbeat packet\n");
        return;
    }
    if (heartbeat.timeslot >= GCHMAC_SLOTFRAME_TIMESLOTS) {
        LOG_ERR("heartbeat_callback: Received timeslot exceeds slotframe length\n");
        return;
    }
    LOG_DBG("received heartbeat with colmap %u timeslot %u from ", heartbeat.colmap, heartbeat.timeslot);
    LOG_DBG_LLADDR(src);
    LOG_DBG_("\n");
    if (heartbeat.timeslot == 0) {
        LOG_ERR("heartbeat_callback: Invalid neighbor timeslot '0'\n");
        return;
    }

    gchmac_nbr_t *nbr = NULL;
    for (unsigned int i = 0; i < GCHMAC_MAX_NEIGHBORS; i++) {
        gchmac_nbr_t *n = &neighbors[i];
        if (n->ttl == 0) {
            nbr = &neighbors[i];
        }
        if (n->ttl > 0 && linkaddr_cmp(&n->addr, src)) {
            nbr = &neighbors[i];
            break;
        }
    }

    if (nbr == NULL) {
        LOG_ERR("maximum number of neighbors reached.\n");
        return;
    }
    combined_nbrmap_buf |= heartbeat.nbrmap;

    if ((heartbeat.colmap & (1 << tx_timeslot)) == 1) {
        LOG_INFO("Collision detected, will change TX slot next heartbeat.\n");
        detected_collision = true;
    }

    linkaddr_copy(&nbr->addr, src);
    nbr->ttl = GCHMAC_HEARTBEAT_TTL;
    nbr->timeslot = heartbeat.timeslot - 1;
}


void broadcast_heartbeat() {
    // public collision map
    colmap_t pub_nbrmap = (1 << tx_timeslot);
    colmap_t pub_colmap = 0;
    colmap_t nbr_bit = 0;
    for (unsigned int i = 0; i < GCHMAC_MAX_NEIGHBORS; i++) {
        gchmac_nbr_t *nbr = &neighbors[i];
        if (nbr->ttl > 0) {
            LOG_DBG("found neighbor ");
            LOG_DBG_LLADDR(&nbr->addr);
            LOG_DBG_(" on timeslot %u ttl %u\n", i, nbr->ttl);
            nbr_bit = 1 << (nbr->timeslot + 1);
            if ((pub_nbrmap & nbr_bit) != 0) {
                if(nbr->timeslot == tx_timeslot) {
                    LOG_INFO("Collision detected, will change TX slot next heartbeat.\n");
                    detected_collision = true;
                }
                pub_colmap |= nbr_bit;
            }
            pub_nbrmap |= nbr_bit;
            nbr->ttl--;
        }
    }

    gchmac_heartbeat_t heartbeat = {
        .colmap = pub_colmap,
        .nbrmap = pub_nbrmap,
        .timeslot = tx_timeslot
    };
    LOG_DBG("public colmap: %u\n", heartbeat.colmap);
    LOG_DBG("public timeslot: %u\n", heartbeat.timeslot);
    memcpy(heartbeat_buf + sizeof(uint8_t), &heartbeat, sizeof(heartbeat));
    LOG_INFO("packet: ");
    print_bytes_hex(heartbeat_buf, GCHMAC_HEARTBEAT_PACKET_LEN);
    nullnet_buf = heartbeat_buf;
    nullnet_len = GCHMAC_HEARTBEAT_PACKET_LEN;
    NETSTACK_NETWORK.output(NULL);
}

void input_cb(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
{
    gchmac_packet_type type = *(uint8_t*)data;
    const void *payload = data + sizeof(uint8_t);
    uint16_t payload_len = len - sizeof(uint8_t);

    LOG_DBG("Data len = %d\n", len);

    switch (type) {
        case GCHMAC_PACKET_TYPE_DATA:
			LOG_INFO("HERE1\n");
            if (upper_input_callback != NULL){
                LOG_DBG(" data correct\n");
                upper_input_callback(payload, payload_len, src, dest);
            }
            break;
        case GCHMAC_PACKET_TYPE_HEARTBEAT:
			LOG_INFO("HERE2\n");
            LOG_DBG(" heartbeat\n");
            heartbeat_callback(payload, payload_len, src, dest);
            break;
    }
}


PROCESS_THREAD(gchmac_heartbeat_proc, ev, data) {
    PROCESS_BEGIN();
    LOG_DBG("Started heartbeat thread.\n");

    static struct etimer scan_timer;


    etimer_set(&scan_timer, random_rand() % GCHMAC_HEARTBEAT_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&scan_timer));
    etimer_set(&scan_timer, GCHMAC_HEARTBEAT_INTERVAL + (random_rand() % GCHMAC_HEARTBEAT_DEVIATION) - (GCHMAC_HEARTBEAT_DEVIATION / 2));
    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&scan_timer));
        combined_nbrmap = combined_nbrmap_buf;
        combined_nbrmap_buf = 0;
        if (tx_timeslot == 0 || detected_collision) {
            LOG_DBG("Selecting a tx timeslot.\n");
            choose_tx_slot();
            detected_collision = false;
        }
        LOG_DBG("broadcast heartbeat\n");
        broadcast_heartbeat();
        etimer_reset(&scan_timer);
    }

    PROCESS_END();
}

void gchmac_init() {
    initialize_tsch_schedule();
    // Cooja motes use link addresses to distinguish nodes, but Sky motes use the node_id global. Hence the following
    #if COOJA == 1
        bool is_coordinator = linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr);
    #else
        bool is_coordinator = node_id == 1;
        tsch_set_coordinator(1);
        tsch_set_pan_secured(0);
        tsch_set_eb_period(4 * CLOCK_SECOND);
    #endif

    tsch_set_coordinator(is_coordinator);
    nullnet_buf = payload_buf;
    nullnet_set_input_callback(input_cb);
    if (is_coordinator) {
        LOG_INFO("Current node is TSCH coordinator\n");
    }
    process_start(&gchmac_heartbeat_proc, NULL);
}

void gchmac_set_input_callback(gchmac_input_callback_t cb) {
    upper_input_callback = cb;
}

void gchmac_broadcast(void *data, uint16_t len) {
    payload_buf[0] = GCHMAC_PACKET_TYPE_DATA;
    memcpy(payload_buf + sizeof(uint8_t), data, len);

    nullnet_buf = payload_buf;
    nullnet_len = len + sizeof(uint8_t);
    NETSTACK_NETWORK.output(NULL);
}


/*---------------------------------------------------------------------------*/
