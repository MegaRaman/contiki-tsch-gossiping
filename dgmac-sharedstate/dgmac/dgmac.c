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

#include "dgmac.h"
#include "etimer.h"
#include "dgmac-conf.h"
#include "dgmac-debug.h"
#include "dgmac-types.h"
#include "dgmac-packet.h"

#include "linkaddr.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
//#include "net/mac/tsch/tsch.h"
#include "lib/random.h"
#include "packetbuf.h"
#include "process.h"

//#include "tsch-schedule.h"
#include "watchdog.h"
//#include <cstdint>
#include <stdint.h>

#include "sys/log.h"
#define LOG_MODULE "DG-MAC"
#define LOG_LEVEL LOG_LEVEL_DBG

#if COOJA != 1
#include "sys/node-id.h"
#endif

#include <string.h>

/* Configuration */
#if COOJA == 1
static linkaddr_t coordinator_addr =  {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
#endif

static dgmac_nbr_t neighbors[DGMAC_MAX_NEIGHBORS] = { 0 };
static colmap_t combined_nbrmap = 0;
static colmap_t combined_nbrmap_buf = 0;

static uint16_t tx_timeslot = 0;
static bool detected_collision = false;
bool is_coordinator = false;

static uint8_t heartbeat_buf[DGMAC_HEARTBEAT_PACKET_LEN] = { 0 };
static uint8_t payload_buf[DGMAC_PACKET_MAX_LEN] = { 0 };

static dgmac_input_callback_t upper_input_callback = NULL;


PROCESS(dgmac_heartbeat_proc, "heartbeat thread");


/*---------------------------------------------------------------------------*/

int
dgmac_callback_packet_ready(void)
{
    dgmac_packet_type type = *(uint8_t*)packetbuf_dataptr();
    uint16_t timeslot;
    switch(type) {
        case DGMAC_PACKET_TYPE_DATA:
            timeslot = tx_timeslot;
            break;
        default:
            timeslot = 0;
            break;
    }

    LOG_DBG("Broadcasting on timeslot %u: \n", timeslot);
    print_bytes_hex(packetbuf_dataptr(), packetbuf_datalen());
    return 0;
}

/*---------------------------------------------------------------------------*/

void choose_tx_slot() {
    if (~combined_nbrmap == 1) {
        LOG_ERR("no more free timeslots\n");
    }
    if(!is_coordinator){
        return;
    }
    uint16_t new_slot = 1 + random_rand() % DGMAC_MAX_NEIGHBORS;


    while ( (new_slot == tx_timeslot) ||((combined_nbrmap & (1 << new_slot)) != 0) ) {
        new_slot = 1 + (new_slot + 1) % DGMAC_MAX_NEIGHBORS;
    }


    
    tx_timeslot = new_slot;
    LOG_DBG("Transmission timeslot: %u\n", tx_timeslot);
 

}

void heartbeat_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    is_coordinator = true;
    static dgmac_heartbeat_t heartbeat;

    if (!parse_dgmac_heartbeat(data, len, &heartbeat)) {
        LOG_ERR("dgmac: Failed to parse heartbeat packet\n");
        return;
    }
    if (heartbeat.timeslot >= DGMAC_SLOTFRAME_TIMESLOTS) {
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

    dgmac_nbr_t *nbr = NULL;
    for (unsigned int i = 0; i < DGMAC_MAX_NEIGHBORS; i++) {
        dgmac_nbr_t *n = &neighbors[i];
        if (n->ttl == 0) {
            LOG_INFO("Added new neighbor.\n");
            nbr = &neighbors[i];
            break;
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
    nbr->ttl = DGMAC_HEARTBEAT_TTL;
    nbr->timeslot = heartbeat.timeslot - 1;
}


void broadcast_heartbeat() {
    if (!is_coordinator){
        return;
    }
    // public collision map
    colmap_t pub_nbrmap = (1 << tx_timeslot);
    colmap_t pub_colmap = 0;
    colmap_t nbr_bit = 0;
    for (unsigned int i = 0; i < DGMAC_MAX_NEIGHBORS; i++) {
        dgmac_nbr_t *nbr = &neighbors[i];
        if (nbr->ttl > 0) {
            LOG_DBG("found neighbor ");
            LOG_DBG_LLADDR(&nbr->addr);
            LOG_DBG_(" on timeslot %u ttl %u\n", i, nbr->ttl);
            nbr_bit = 1 << (nbr->timeslot + 1);
            LOG_INFO("Updating collision map with neighbor bit %u.\n", nbr_bit);
            if ((pub_nbrmap & nbr_bit) != 0) {
                if(nbr->timeslot == tx_timeslot) {
                    LOG_INFO("Collision detected, will change TX slot next heartbeat.\n");
                    detected_collision = true;
                }
                pub_colmap |= nbr_bit;
            }
            pub_nbrmap |= nbr_bit;
            nbr->ttl--;
            LOG_INFO("Updating collision map %u.\n", pub_colmap);
        }
    }

    dgmac_heartbeat_t heartbeat = {
        .colmap = pub_colmap,
        .nbrmap = pub_nbrmap,
        .timeslot = tx_timeslot
    };
    LOG_DBG("Sending public colmap: %u\n", heartbeat.colmap);
    LOG_DBG("Sending public timeslot: %u\n", heartbeat.timeslot);
    memcpy(heartbeat_buf + sizeof(uint8_t), &heartbeat, sizeof(heartbeat));
    LOG_INFO("packet: ");
    print_bytes_hex(heartbeat_buf, DGMAC_HEARTBEAT_PACKET_LEN);
    nullnet_buf = heartbeat_buf;
    nullnet_len = DGMAC_HEARTBEAT_PACKET_LEN;
    NETSTACK_NETWORK.output(NULL);
}

void input_cb(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
{
    dgmac_packet_type type = *(uint8_t*)data;
    const void *payload = data + sizeof(uint8_t);
    uint16_t payload_len = len - sizeof(uint8_t);

    LOG_DBG("Data len = %d\n", len);

    switch (type) {
        case DGMAC_PACKET_TYPE_DATA:
            if (upper_input_callback != NULL){
                LOG_DBG(" data correct\n");
                upper_input_callback(payload, payload_len, src, dest);
            }
            break;
        case DGMAC_PACKET_TYPE_HEARTBEAT:
            LOG_DBG(" heartbeat\n");
            heartbeat_callback(payload, payload_len, src, dest);
            break;
    }
}


PROCESS_THREAD(dgmac_heartbeat_proc, ev, data) {
    PROCESS_BEGIN();
    LOG_DBG("Started heartbeat thread.\n");

    static struct etimer scan_timer;


    etimer_set(&scan_timer, random_rand() % DGMAC_HEARTBEAT_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&scan_timer));
    etimer_set(&scan_timer, DGMAC_HEARTBEAT_INTERVAL + (random_rand() % DGMAC_HEARTBEAT_DEVIATION) - (DGMAC_HEARTBEAT_DEVIATION / 2));
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

void dgmac_init() {
    //initialize_tsch_schedule();
    // Cooja motes use link addresses to distinguish nodes, but Sky motes use the node_id global. Hence the following
    #if COOJA == 1
        is_coordinator = linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr);
    #else
        is_coordinator = node_id == 1;
    #endif

    tx_timeslot = (is_coordinator)? 1 : 0;

    //tsch_set_coordinator(is_coordinator);
    nullnet_buf = payload_buf;
    nullnet_set_input_callback(input_cb);
    if (is_coordinator) {
        LOG_INFO("Current node is TSCH coordinator\n");
    }
    process_start(&dgmac_heartbeat_proc, NULL);
}

void dgmac_set_input_callback(dgmac_input_callback_t cb) {
    upper_input_callback = cb;
}

void dgmac_broadcast(void *data, uint16_t len) {
    payload_buf[0] = DGMAC_PACKET_TYPE_DATA;
    memcpy(payload_buf + sizeof(uint8_t), data, len);

    nullnet_buf = payload_buf;
    nullnet_len = len + sizeof(uint8_t);
    NETSTACK_NETWORK.output(NULL);
}

uint16_t get_waiting_time(){
    if(detected_collision || tx_timeslot == 0){
        return 0;
    }
    return tx_timeslot;
}


/*---------------------------------------------------------------------------*/
