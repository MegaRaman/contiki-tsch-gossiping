#ifndef SHAREDSTATE_H_
#define SHAREDSTATE_H_

#define PKT_SIZE_BYTES 32
// pkt id format: byte 0 - node_id, byte 1 - msg_id
#define PKT_ID_SIZE_BYTES 2
#define TIMESTAMP_SIZE_BYTES (RTIMER_CLOCK_SIZE)
#define OVERLOAD_SIZE_BYTES 1
#define PKT_DATA_SIZE_BYTES (PKT_SIZE_BYTES - PKT_ID_SIZE_BYTES - TIMESTAMP_SIZE_BYTES - OVERLOAD_SIZE_BYTES)

/* input buffer size is at most as big as the cache */
#define INPUT_BUF_SIZE 20
#define CACHE_SIZE 32
#define OUTPUT_BUF_SIZE 2

#define NETSTACK_CONF_NETWORK nullnet_driver
#define NETSTACK_CONF_WITH_NULLNET 1

#define LOG_CONF_LEVEL_NULLNET LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_INFO

#include <stdint.h>

#include "sys/rtimer.h"

typedef struct
{
	uint8_t pkt_id[PKT_ID_SIZE_BYTES];
	rtimer_clock_t tstamp;
	uint8_t overload;
	uint8_t data[PKT_DATA_SIZE_BYTES];
} sharedstate_pkt_t;

typedef struct
{
	sharedstate_pkt_t input_buf[INPUT_BUF_SIZE];
	sharedstate_pkt_t output_buf[OUTPUT_BUF_SIZE];
	sharedstate_pkt_t cache[CACHE_SIZE];
	// either that shi(workaround) or hashmap
	bool cache_occupied_index[CACHE_SIZE];

	uint8_t input_buf_cnt;
	uint8_t cache_entries_cnt;

	uint8_t msgs_dropped_nr;
	uint16_t overload_cumulative;
	int node_id;
} sharedstate_t;

void init_sharedstate(sharedstate_t *sharedstate, int node_id);
void sharedstate_rx(sharedstate_t *sharedstate, sharedstate_pkt_t *pkt);
void sharedstate_tx(sharedstate_t *sharedstate);
void sharedstate_app_send(sharedstate_t *sharedstate, void *data, uint16_t len, uint8_t rx_id);

static inline int get_pkt_id(uint8_t pkt_id[PKT_ID_SIZE_BYTES])
{
	return pkt_id[1] << 8 | pkt_id[0];
}

#endif /* PROJECT_CONF_H_ */
