#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/etimer.h"
#include "sys/log.h"
#include "random.h"
#include "sharedstate.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Log configuration */
#define LOG_MODULE "SharedState" // LOG name
#define LOG_LEVEL LOG_LEVEL_INFO // LOG info type

static SharedState_item_t cache[SHAREDSTATE_MAX_ITEMS];
static int cache_count = 0;

/* Simple djb2 hash function for strings */
unsigned long hash(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

static int find_in_cache(uint32_t id)
{
    for (int i = 0; i < cache_count; i++)
        if (cache[i].id == id)
            return i;
    return -1;
}

static void cache_insert(SharedState_item_t *item)
{
    LOG_INFO("Cache insert req id: '%ui' value: '%s'\n", item->id, item->data);
    int i = find_in_cache(item->id);

    if (i < 0 && cache_count < SHAREDSTATE_MAX_ITEMS)
    {
        LOG_INFO("Cache inserted id: '%ui' in position: '%d'\n", item->id, cache_count);
        cache[cache_count++] = *item;
        return;
    }
    else if (i >= 0 && item->ts > cache[i].ts)
    {
        LOG_INFO("Cache updated id: '%ui' in position: '%d'\n", item->id, i);
        cache[i] = *item;
        return;
    }
    else
    {
        LOG_INFO("Cache full, cannot insert id: '%ui'\n", item->id);
        return;
    }
}

static void recv_callback(const void *data,
                          uint16_t datalen,
                          const linkaddr_t *sender_addr,
                          const linkaddr_t *receiver_addr)
{
    LOG_INFO("SharedState recv callback\n");
    if (datalen == sizeof(SharedState_item_t))
    {
        // for (int i = 0; i < LINKADDR_SIZE; i++)
        //     LOG_INFO_("%02x", sender_addr->u8[i]);
        LOG_INFO_("Received data from id: '%ui' value: '%s'\n",
                  ((SharedState_item_t *)data)->id,
                  ((SharedState_item_t *)data)->data);
        SharedState_item_t item;
        memcpy(&item, data, datalen);
        cache_insert(&item);
    }
    else
    {
        LOG_WARN("Received invalid data size: '%d'\n", datalen);
        return;
    }
}

void sharedstate_init(void)
{
    LOG_INFO("SharedState init\n");
    nullnet_set_input_callback(recv_callback);
    NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 18);
    nullnet_buf = (uint8_t *)&cache[0];
    nullnet_len = sizeof(SharedState_item_t);
    LOG_INFO("SharedState initialized\n");
}

void sharedstate_periodic()
{
    LOG_INFO("SharedState periodic\n");
    if (cache_count == 0)
        return;

    int r = rand() % cache_count;
    SharedState_item_t *item = &cache[r];
    LOG_INFO("Periodic broadcast of id: '%ui' value: '%s'\n",
             item->id, item->data);
    nullnet_buf = (uint8_t *)item;
    nullnet_len = sizeof(SharedState_item_t);
    NETSTACK_NETWORK.output(NULL);
}

void sharedstate_put(const char *key, const char *value)
{
    LOG_INFO("SharedState_put key: '%s' value: '%s'\n", key, value);
    SharedState_item_t item;
    item.id = hash(key);
    item.ts = clock_seconds();
    item.len = strlen(value);
    memcpy(item.data, value, item.len + 1);
    cache_insert(&item);
}

const char *sharedstate_get(const char *key)
{
    uint32_t id = hash(key);
    int idx = find_in_cache(id);
    if (idx >= 0)
    {
        LOG_INFO("SharedState_get key: '%s' found value: '%s'\n", key, cache[idx].data);
        return (char *)cache[idx].data;
    }
    return NULL;
}