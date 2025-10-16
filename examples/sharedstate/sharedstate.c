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

static sharedstate_t sharedstate;
uint8_t msg_id = 0;

inline int get_pkt_id(uint8_t pkt_id[PKT_ID_SIZE_BYTES])
{
	return pkt_id[1] << 8 | pkt_id[0];
}

inline int get_pkt_tstamp(uint8_t tstamp[TIMESTAMP_SIZE_BYTES])
{
	return tstamp[1] << 8 | tstamp[0];
}

// TODO: do we need a hashmap lookup?
int inputbuf_contains(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt)
{
	int pkt_id = get_pkt_id(pkt->pkt_id);
	for (int i = 0; i < sharedstate->msgs_rx_nr; i++)
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
	for (int i = 0; i < sharedstate->cache_entries_nr; i++)
	{
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
		LOG_INFO("Attempt to remove non-existing entry from cache\n");
		return;
	}
	sharedstate->cache_occupied_index[cache_index] = false;
	sharedstate->cache_entries_nr--;
}

bool cache_add(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt)
{
	if (sharedstate->cache_entries_nr == CACHE_SIZE)
	{
		return false;
	}
	int i = 0;
	while (!sharedstate->cache_occupied_index[i++])
		;
	sharedstate->cache[i] = *pkt;
	sharedstate->cache_occupied_index[i] = true;
	sharedstate->cache_entries_nr++;

	return true;
}

void cache_get_random_entries(sharedstate_t *sharedstate, uint8_t entry_nr,
							  int entries[entry_nr])
{
	if (sharedstate->cache_entries_nr < entry_nr)
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
	for (int i = sharedstate->cache_entries_nr - 1; i > 0; i--)
	{
		int j = random_rand() % (i + 1);
		int temp = occupied_indices[i];
		occupied_indices[i] = occupied_indices[j];
		occupied_indices[j] = temp;
	}
	memcpy(entries, occupied_indices, entry_nr);
}

static void recv_callback(const void *data,
						  uint16_t datalen,
						  const linkaddr_t *src,
						  const linkaddr_t *dest)
{
	if (datalen == sizeof(sharedstate_pkt_t))
	{
		LOG_INFO("received id: %u data: %u\n",
				 ((sharedstate_pkt_t *)data)->pkt_id[1],
				 ((sharedstate_pkt_t *)data)->data[0]);
		sharedstate_rx(&sharedstate, (sharedstate_pkt_t *)data);
	}
	else
	{
		LOG_WARN("Received invalid data size: '%d'\n", datalen);
		return;
	}
}

void init_sharedstate(sharedstate_t *sharedstate, int node_id)
{
	memset(sharedstate, 0, sizeof(sharedstate_t));

	sharedstate->msgs_rx_nr = 0;
	sharedstate->cache_entries_nr = 0;
	sharedstate->node_id = node_id;
	sharedstate->tx_local = false;

	sharedstate->msgs_dropped_nr = 0;
	sharedstate->overload_cumulative = 0;

	for (int i = 0; i < CACHE_SIZE; i++)
	{
		sharedstate->cache_occupied_index[i] = false;
	}
	random_init(0);

	nullnet_set_input_callback(recv_callback);
	NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 18);
	nullnet_buf = (uint8_t *)&(sharedstate->output_buf[0]);
	// nullnet_buf = (uint8_t *)&(sharedstate->local_entry);
	nullnet_len = sizeof(sharedstate_pkt_t);
	// nullnet_len = sizeof(sharedstate_pkt_t) * OUTPUT_BUF_SIZE;
	LOG_INFO("SharedState initialized\n");
}

void sharedstate_rx(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt)
{
	if (sharedstate->msgs_rx_nr == INPUT_BUF_SIZE)
	{
		LOG_INFO("Input buffer full, dropping pkt id: %u data: %u\n",
				 pkt->pkt_id[1],
				 pkt->data[0]);
		sharedstate->msgs_dropped_nr++;
		return;
	}
	int contains_i = inputbuf_contains(sharedstate, pkt);
	if (contains_i >= 0)
	{
		LOG_INFO("Input buffer already contains pkt id: %u data: %u\n",
				 pkt->pkt_id[1],
				 pkt->data[0]);
		sharedstate_pkt_t inbuf_pkt = sharedstate->input_buf[contains_i];
		if (inbuf_pkt.tstamp < pkt->tstamp)
		{
			sharedstate->input_buf[contains_i] = *pkt;
		}
	}
	else
	{
		LOG_INFO("Adding pkt id: %u data: %u to input buffer\n",
				 pkt->pkt_id[1],
				 pkt->data[0]);
		sharedstate->input_buf[sharedstate->msgs_rx_nr] = *pkt;
		sharedstate->msgs_rx_nr++;
	}
}

void sharedstate_app_send(sharedstate_t *sharedstate, void *data, uint16_t len)
{
	if (len > PKT_DATA_SIZE_BYTES)
	{
		LOG_INFO("Sent more than max data len: %u\n", PKT_DATA_SIZE_BYTES);
		return;
	}

	sharedstate->local_entry.overload = 0;
	sharedstate->local_entry.tstamp = RTIMER_NOW();
	sharedstate->local_entry.pkt_id[0] = sharedstate->node_id;
	sharedstate->local_entry.pkt_id[1] = msg_id++;
	memcpy(sharedstate->local_entry.data, data, len);
	sharedstate->tx_local = true;

	LOG_INFO("App send id: %u data: %u\n",
			 sharedstate->local_entry.pkt_id[1],
			 sharedstate->local_entry.data[0]);
}

void sharedstate_update_cache(sharedstate_t *sharedstate)
{
	for (int i = 0; i < sharedstate->msgs_rx_nr; i++)
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
	if (CACHE_SIZE - sharedstate->cache_entries_nr < sharedstate->msgs_rx_nr)
	{
		int entries_needed = sharedstate->msgs_rx_nr -
							 (CACHE_SIZE - sharedstate->cache_entries_nr);
		int random_entries_indices[INPUT_BUF_SIZE];
		cache_get_random_entries(sharedstate, entries_needed, random_entries_indices);
		for (int i = 0; i < entries_needed; i++)
		{
			cache_remove(sharedstate, random_entries_indices[i]);
		}
	}
	for (int i = 0; i < sharedstate->msgs_rx_nr; i++)
	{
		// if we didn't have this input buf entry in the cache
		if (sharedstate->input_buf[i].pkt_id[0] != 0)
		{
			cache_add(sharedstate, &sharedstate->input_buf[i]);
		}
	}
	sharedstate->msgs_rx_nr = 0;
}

void sharedstate_tx(sharedstate_t *sharedstate)
{
	sharedstate_update_cache(sharedstate);

	int i = 0;
	if (sharedstate->tx_local)
	{
		LOG_INFO("Tx local entry id: %u data: %u\n",
				 sharedstate->local_entry.pkt_id[1],
				 sharedstate->local_entry.data[0]);
		sharedstate->tx_local = false;
		sharedstate->output_buf[0] = sharedstate->local_entry;
		i++;
	}
	if (sharedstate->cache_entries_nr < OUTPUT_BUF_SIZE - 1)
	{
		for (; i < sharedstate->cache_entries_nr; i++)
		{
			sharedstate->output_buf[i] = sharedstate->cache[i];
		}

		LOG_INFO("Broadcasting id: %u data: %u\n",
				 sharedstate->output_buf[0].pkt_id[1],
				 sharedstate->output_buf[0].data[0]);

		sharedstate->msgs_dropped_nr = 0;
		sharedstate->overload_cumulative = 0;

		NETSTACK_NETWORK.output(NULL);
		return;
	}

	int entries[OUTPUT_BUF_SIZE];
	cache_get_random_entries(sharedstate, OUTPUT_BUF_SIZE - i, entries);
	for (; i < OUTPUT_BUF_SIZE; i++)
	{
		sharedstate->output_buf[i] = sharedstate->cache[entries[i]];
	}

	LOG_INFO("Broadcasting id: %u data: %u\n",
			 sharedstate->output_buf[0].pkt_id[1],
			 sharedstate->output_buf[0].data[0]);

	sharedstate->msgs_dropped_nr = 0;
	sharedstate->overload_cumulative = 0;

	NETSTACK_NETWORK.output(NULL);
}

PROCESS(sharedstate_process, "Sharedstate process");
AUTOSTART_PROCESSES(&sharedstate_process);

PROCESS_THREAD(sharedstate_process, ev, data)
{
	static struct etimer periodic_timer;
	static int sens_val = 25;

	PROCESS_BEGIN();

	init_sharedstate(&sharedstate, linkaddr_node_addr.u8[1]);

	etimer_set(&periodic_timer, CLOCK_SECOND * 10);

	while (1)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

		sens_val += (rand() % 3) - 1;
		LOG_INFO("Sensor SS put: %d\n", sens_val);

		sharedstate_app_send(&sharedstate, &sens_val, sizeof(sens_val));
		sharedstate_tx(&sharedstate);

		etimer_reset(&periodic_timer);
	}

	PROCESS_END();
}
