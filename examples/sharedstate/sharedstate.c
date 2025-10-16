#include <string.h>
#include <stdbool.h>

#include "sharedstate.h"

#include "contiki.h"
#include "net/netstack.h"
#include "sys/log.h"
#include "lib/random.h"

#define LOG_MODULE "Sharedstate"

uint8_t msg_id = 0;

inline int get_pkt_id(uint8_t pkt_id[PACKET_ID_SIZE_BYTES]) {
	return pkt_id[1] << 8 | pkt_id[0];
}

inline int get_pkt_tstamp(uint8_t tstamp[TIMESTAMP_SIZE_BYTES]) {
	return tstamp[1] << 8 | tstamp[0];
}

// TODO: do we need a hashmap lookup?
int inputbuf_contains(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt) {
	int pkt_id = get_pkt_id(pkt->pkt_id);
	for (int i = 0; i < sharedstate->msgs_rx_nr; i++) {
		if (get_pkt_id(sharedstate->input_buf[i].pkt_id) == pkt_id) {
			return i;
		}
	}
	return -1;
}

// TODO: do we need a hashmap lookup?
int cache_contains(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt) {
	int pkt_id = get_pkt_id(pkt->pkt_id);
	for (int i = 0; i < sharedstate->cache_entries_nr; i++) {
		if (get_pkt_id(sharedstate->cache[i].pkt_id) == pkt_id) {
			return i;
		}
	}
	return -1;
}

void cache_remove(sharedstate_t *sharedstate, int cache_index) {
	if (!sharedstate->cache_occupied_index[cache_index]) {
		LOG_INFO("Attempt to remove non-existing entry from cache\n")
		return;
	}
	sharedstate->cache_occupied_index[cache_index] = false;
	sharedstate->cache_entries_nr--;
}

/* cache can be accessed concurrently by an app and by an input buffer during an
 * active thread */
bool cache_add(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt) {
	if (sharedstate->cache_entries_nr == CACHE_SIZE) {
		return false;
	}
	int i = 0;
	while (!sharedstate->cache_occupied_index[i++]);
	sharedstate->cache[i] = *pkt;
	sharedstate->cache_occupied_index[i] = true;
	sharedstate->cache_entries_nr++;
}

void cache_get_random_entries(sharedstate_t *sharedstate, uint8_t entry_nr,
		int entries[entry_nr]) {
	int occupied_indices[CACHE_SIZE];
	for (int i = 0, j = 0; i < CACHE_SIZE; i++) {
		if (sharedstate->cache_occupied_index[i]) {
			occupied_indices[j++] = i;
		}
	}
	// random shuffle and get entry_nr first elements
    for (int i = sharedstate->cache_entries_nr - 1; i > 0; i--) {
        int j = random_rand() % (i + 1);
        int temp = occupied_indices[i];
        occupied_indices[i] = occupied_indices[j];
        occupied_indices[j] = temp;
    }
	memcpy(entries, occupied_indices, entry_nr);
}

void init_sharedstate(sharedstate_t *sharedstate, int node_id) {
	sharedstate->msgs_rx_nr = 0;
	sharedstate->cache_entries_nr = 0;
	sharedstate->node_id = node_id;
	sharedstate->tx_local = false;

	sharedstate->msgs_dropped_nr = 0;
	sharedstate->overload_cumulative = 0;

	for (int i = 0; i < CACHE_SIZE; i++) {
		sharedstate->cache_occupied_index[i] = false;
	}
	random_init(0);
}

void sharedstate_rx(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt) {
	if (sharedstate->msgs_rx_nr == INPUT_BUF_SIZE) {
		sharedstate->msgs_dropped_nr++;
		return;
	}
	int contains_i = inputbuf_contains(sharedstate, pkt);
	if (contains_i >= 0) {
		sharedstate_pkt_t inbuf_pkt = sharedstate->input_buf[contains_i];
		if (get_pkt_tstamp(inbuf_pkt.tstamp) < get_pkt_tstamp(pkt->tstamp)) {
			sharedstate->input_buf[contains_i] = *pkt;
		}
	}
	else {
		sharedstate->input_buf[sharedstate->msgs_rx_nr] = *pkt;
		sharedstate->msgs_rx_nr++;
	}
}

void sharedstate_app_send(sharedstate_t *sharedstate, void *data, uint16_t len) {
	if (len > PKT_DATA_SIZE_BYTES) {
		LOG_INFO("Sent more than max data len: %u\n", PKT_DATA_SIZE_BYTES);
		return;
	}

	sharedstate->local_entry.overload = 0;
	sharedstate->local_entry.tstamp = RTIMER_NOW();
	sharedstate->local_entry.pkt_id[0] = sharedstate->node_id;
	sharedstate->local_entry.pkt_id[1] = msg_id++;
	sharedstate->tx_local = true;
}

void sharedstate_update_cache(sharedstate_t *sharedstate) {
	for (int i = 0; i < sharedstate->msgs_rx_nr; i++) {
		int j = cache_contains(sharedstate, &(sharedstate->input_buf[i]));
		if (j >= 0) {
			cache_remove(sharedstate, j);
			// if input buffer entry needs to be removed, we'll just omit it by
			// setting node_id = 0
			sharedstate->input_buf[i].pkt_id[0] = 0;
		}
	}
	if (CACHE_SIZE - sharedstate->cache_entries_nr < sharedstate->msgs_rx_nr) {
		int entries_needed = sharedstate->msgs_rx_nr -
			(CACHE_SIZE - sharedstate->cache_entries_nr);
		int random_entries_indices[INPUT_BUF_SIZE];
		cache_get_random_entries(sharedstate, entries_needed, random_entries_indices);
		for (int i = 0; i < entries_needed; i++) {
			cache_remove(sharedstate, random_entries_indices[i]);
		}
	}
	for (int i = 0; i < sharedstate->msgs_rx_nr; i++) {
		// if we didn't have this input buf entry in the cache
		if (sharedstate->input_buf[i].pkt_id[0] != 0) {
			cache_add(sharedstate->input_buf[i]);
		}
	}
	sharedstate->msgs_rx_nr = 0;
}

void sharedstate_tx(sharedstate_t *sharedstate) {
	sharedstate_update_cache(sharedstate);

	int i = 0;
	if (sharedstate->tx_local) {
		sharedstate->tx_local = false;
		sharedstate->output_buf[0] = sharedstate->local_entry;
		i++;
	}
	int entries[OUTPUT_BUF_SIZE];
	cache_get_random_entries(sharedstate, OUTPUT_BUF_SIZE - i, entries);
	for (; i < OUTPUT_BUF_SIZE; i++) {
		sharedstate->output_buf[i] = sharedstate->cache[entries[i]];
	}

	sharedstate->msgs_dropped_nr = 0;
	sharedstate->overload_cumulative = 0;
}

PROCESS(sharedstate_process, "Sharedstate process");
AUTOSTART_PROCESSES(&sharedstate_process);

PROCESS_THREAD(sharedstate_process, ev, data)
{
	PROCESS_BEGIN();
}

