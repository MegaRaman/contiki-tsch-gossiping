#include "contiki.h"
#include "sharedstate.h"

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/log.h"
#include "lib/random.h"
#include "sys/etimer.h"

#define LOG_MODULE "Sharedstate"
#define LOG_LEVEL LOG_LEVEL_INFO // LOG info type

#define NODES_CNT	50

static sharedstate_t sharedstate;
uint8_t msg_id = 0;

// TODO: do we need a hashmap lookup?
int inputbuf_contains(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt)
{
	int pkt_id = get_pkt_id(pkt->pkt_id);
	for (int i = 0; i < sharedstate->input_buf_cnt; i++)
	{
		if (get_pkt_id(sharedstate->input_buf[i].pkt_id) == pkt_id)
		{
			return i;
		}
	}
	return -1;
}

// TODO: do we need a hashmap lookup?
int cache_contains(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt)
{
	int pkt_id = get_pkt_id(pkt->pkt_id);
	for (int i = 0; i < CACHE_SIZE; i++)
	{
		if (!sharedstate->cache_occupied_index[i]) {
			continue;
		}
		if (get_pkt_id(sharedstate->cache[i].pkt_id) == pkt_id)
		{
			return i;
		}
	}
	return -1;
}

void cache_remove(sharedstate_t *sharedstate, int cache_index)
{
	if (!sharedstate->cache_occupied_index[cache_index])
	{
		// LOG_INFO("Attempt to remove non-existing entry from cache: %d\n", cache_index);
		return;
	}
	sharedstate->cache_occupied_index[cache_index] = false;
	sharedstate->cache_entries_cnt--;
}

bool cache_add(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt)
{
	if (sharedstate->cache_entries_cnt == CACHE_SIZE)
	{
		return false;
	}
	for (int i = 0; i < CACHE_SIZE; i++) {
		if (!sharedstate->cache_occupied_index[i]) {
			sharedstate->cache[i] = *pkt;
			sharedstate->cache_occupied_index[i] = true;
			sharedstate->cache_entries_cnt++;
			return true;
		}
	}
	// LOG_INFO("Error: no place in cache, but there should be\n");
	return false;
}

void cache_get_random_entries(sharedstate_t *sharedstate, uint8_t entry_nr,
							  int *entries)
{
	if (sharedstate->cache_entries_cnt < entry_nr)
	{
		LOG_WARN("Not enough cache entries to get %u random ones\n", entry_nr);
		return;
	}

	int occupied_indices[CACHE_SIZE];
	for (int i = 0, j = 0; i < CACHE_SIZE; i++)
	{
		if (sharedstate->cache_occupied_index[i])
		{
			occupied_indices[j++] = i;
		}
	}
	// random shuffle and get entry_nr first elements
	for (int i = entry_nr - 1; i > 0; i--)
	{
		int j = random_rand() % (i + 1);
		int temp = occupied_indices[i];
		occupied_indices[i] = occupied_indices[j];
		occupied_indices[j] = temp;
	}
	for (int i = 0; i < entry_nr; i++) {
		entries[i] = occupied_indices[i];
	}
}

static void recv_callback(const void *data,
						  uint16_t datalen,
						  const linkaddr_t *src,
						  const linkaddr_t *dest)
{
	if (datalen == sizeof(sharedstate_pkt_t) * OUTPUT_BUF_SIZE)
	{
		// LOG_INFO("received id: %u data: %u\n",
		// 		 ((sharedstate_pkt_t *)data)->pkt_id[1],
		// 		 ((sharedstate_pkt_t *)data)->data[0]);
		for (int i = 0; i < OUTPUT_BUF_SIZE; i++) {
			sharedstate_rx(&sharedstate, ((sharedstate_pkt_t*)data) + i);
		}
	}
	else
	{
		// LOG_WARN("Received invalid data size: '%d'\n", datalen);
		return;
	}
}

void init_sharedstate(sharedstate_t *sharedstate, int node_id)
{
	memset(sharedstate, 0, sizeof(sharedstate_t));

	sharedstate->input_buf_cnt = 0;
	sharedstate->cache_entries_cnt = 0;
	sharedstate->node_id = node_id;

	sharedstate->msgs_dropped_nr = 0;
	sharedstate->overload_cumulative = 0;

	for (int i = 0; i < CACHE_SIZE; i++)
	{
		sharedstate->cache_occupied_index[i] = false;
	}
	random_init(node_id);

	nullnet_set_input_callback(recv_callback);
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 18);
	// TODO: transmit the whole output buffer
	nullnet_buf = (uint8_t *)&(sharedstate->output_buf);
	nullnet_len = sizeof(sharedstate_pkt_t) * OUTPUT_BUF_SIZE;
	// nullnet_len = sizeof(sharedstate_pkt_t) * OUTPUT_BUF_SIZE;
	LOG_INFO("SharedState initialized\n");
}

void sharedstate_rx(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt)
{
	if (pkt->pkt_id[0] == sharedstate->node_id) {
		LOG_INFO("shst: rx %d %d\n", sharedstate->node_id, pkt->pkt_id[1]);
	}
	if (sharedstate->input_buf_cnt == INPUT_BUF_SIZE)
	{
		// LOG_INFO("Input buffer full, dropping pkt id: %u data: %s\n",
		// 		 pkt->pkt_id[1],
		// 		 pkt->data);
		sharedstate->msgs_dropped_nr++;
		return;
	}
	int contains_i = inputbuf_contains(sharedstate, pkt);
	if (contains_i >= 0)
	{
		// LOG_INFO("Input buffer already contains pkt id: %u data: %s\n",
		// 		 pkt->pkt_id[1],
		// 		 pkt->data);
		sharedstate_pkt_t inbuf_pkt = sharedstate->input_buf[contains_i];
		if (inbuf_pkt.tstamp < pkt->tstamp)
		{
			sharedstate->input_buf[contains_i] = *pkt;
		}
	}
	else
	{
		// LOG_INFO("Adding pkt id: %u data: %s to input buffer\n",
		// 		 pkt->pkt_id[1],
		// 		 pkt->data);
		sharedstate->input_buf[sharedstate->input_buf_cnt] = *pkt;
		sharedstate->input_buf_cnt++;
	}
}

void sharedstate_app_send(sharedstate_t *sharedstate, void *data, uint16_t len, uint8_t rx_id)
{
	if (len > PKT_DATA_SIZE_BYTES)
	{
		// LOG_INFO("Sent more than max data len: %u\n", PKT_DATA_SIZE_BYTES);
		return;
	}

	sharedstate_pkt_t pkt;
	pkt.tstamp = RTIMER_NOW();
	pkt.pkt_id[0] = rx_id;
	pkt.pkt_id[1] = msg_id++;
	pkt.overload = 0;
	memcpy(pkt.data, data, len);
	cache_add(sharedstate, &pkt);
}

void sharedstate_update_cache(sharedstate_t *sharedstate)
{
	for (int i = 0; i < sharedstate->input_buf_cnt; i++)
	{
		int j = cache_contains(sharedstate, &(sharedstate->input_buf[i]));
		if (j >= 0)
		{
			cache_remove(sharedstate, j);
			// if input buffer entry needs to be removed, we'll just omit it by
			// setting node_id = 0
			sharedstate->input_buf[i].pkt_id[0] = 0;
		}
	}
	if (CACHE_SIZE - sharedstate->cache_entries_cnt < sharedstate->input_buf_cnt)
	{
		int entries_needed = sharedstate->input_buf_cnt -
							 (CACHE_SIZE - sharedstate->cache_entries_cnt);
		int random_entries_indices[INPUT_BUF_SIZE];
		cache_get_random_entries(sharedstate, entries_needed, random_entries_indices);
		for (int i = 0; i < entries_needed; i++)
		{
			cache_remove(sharedstate, random_entries_indices[i]);
		}
	}
	for (int i = 0; i < sharedstate->input_buf_cnt; i++)
	{
		// if we didn't have this input buf entry in the cache
		if (sharedstate->input_buf[i].pkt_id[0] != 0)
		{
			cache_add(sharedstate, &sharedstate->input_buf[i]);
		}
	}
	sharedstate->input_buf_cnt = 0;
}

void sharedstate_tx(sharedstate_t *sharedstate)
{
	sharedstate_update_cache(sharedstate);
	if (sharedstate->cache_entries_cnt < OUTPUT_BUF_SIZE) {
		// LOG_INFO("Not enough entries to tx\n");
		return;
	}

	int entries[OUTPUT_BUF_SIZE];
	cache_get_random_entries(sharedstate, OUTPUT_BUF_SIZE, entries);
	for (int i = 0; i < OUTPUT_BUF_SIZE; i++)
	{
		sharedstate->output_buf[i] = sharedstate->cache[entries[i]];
		cache_remove(sharedstate, entries[i]);
	}

	// LOG_INFO("Broadcasting id: %u data: %s\n",
	// 		 sharedstate->output_buf[0].pkt_id[1],
	// 		 sharedstate->output_buf[0].data);

	sharedstate->msgs_dropped_nr = 0;
	sharedstate->overload_cumulative = 0;

	NETSTACK_NETWORK.output(NULL);
}

PROCESS(sharedstate_process, "Sharedstate process");
AUTOSTART_PROCESSES(&sharedstate_process);

PROCESS_THREAD(sharedstate_process, ev, data)
{
	static struct etimer periodic_timer;
	static char sens_val[PKT_DATA_SIZE_BYTES];

	PROCESS_BEGIN();

	init_sharedstate(&sharedstate, linkaddr_node_addr.u8[0]);

	etimer_set(&periodic_timer, (random_rand() % CLOCK_SECOND * 10) + 5);

	while (1)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

		sprintf(sens_val, "%d\n", 25 + (rand() % 3) - 1);
		// LOG_INFO("Sensor SS put: %s\n", sens_val);

		uint8_t rx_id = (random_rand () % NODES_CNT) + 1;
		while (rx_id == sharedstate.node_id) {
			rx_id = (random_rand () % NODES_CNT) + 1;
		}
		LOG_INFO("shst: tx %d %d\n", rx_id, msg_id);
		// account for \0
		sharedstate_app_send(&sharedstate, &sens_val, strlen(sens_val) + 1, rx_id);
		sharedstate_tx(&sharedstate);

		etimer_reset(&periodic_timer);
	}

	PROCESS_END();
}

