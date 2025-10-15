#include "contiki.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/etimer.h"
#include "sys/log.h"
#include "random.h"
#include "sharedstate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sys/clock.h"

/* Log configuration */
#define LOG_MODULE "SharedState" // LOG name
#define LOG_LEVEL LOG_LEVEL_INFO // LOG info type

#define UDP_PORT 8765
#define CACHE_SIZE 16
#define IBUF_SIZE 16
#define OBUF_SIZE 8
#define ROUND_INTERVAL (CLOCK_SECOND * 5)

SharedState_item_t cache[CACHE_SIZE];
static int cache_count;

// static SharedState_item_t iBuffer[IBUF_SIZE];
// static int iBuffer_count;

// static SharedState_item_t oBuffer[OBUF_SIZE];
// static int oBuffer_count;

// static struct simple_udp_connection udp_conn;

void sharedState_periodic()
{
    if (cache_count == 0)
        return;

    int r = rand() % cache_count;
    SharedState_item_t *item = &cache[r];
    LOG_INFO("Periodic broadcast of id: '%ui' value: '%s'\n",
             item->id, item->data);
    NETSTACK_NETWORK.output(NULL);
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
    LOG_INFO("Cache insert id: '%ui' value: '%s'\n", item->id, item->data);
    if (find_in_cache(item->id) >= 0)
    {
        LOG_INFO("Cache insert id: '%ui' already present... replacing\n", item->id);
        cache[cache_count] = *item;
        return;
    }
    if (cache_count < CACHE_SIZE)
    {
        LOG_INFO("Cache insert id: '%ui' in position: '%d'\n", item->id, cache_count);
        cache[cache_count++] = *item;
    }
    else
    {
        LOG_INFO("Cache full, replacing random item with id: '%ui'\n", item->id);
        int r = rand() % CACHE_SIZE;
        cache[r] = *item;
    }
}

// static void udp_rx_callback(struct simple_udp_connection *c,
//                             const uip_ipaddr_t *sender_addr,
//                             uint16_t sender_port,
//                             const uip_ipaddr_t *receiver_addr,
//                             uint16_t receiver_port,
//                             const uint8_t *data,
//                             uint16_t datalen)
// {
//     LOG_INFO("Rx sz: '%d'. from ", datalen);
//     LOG_INFO_6ADDR(sender_addr);
//     LOG_INFO_("\n");

//     if (datalen == 0)
//         return;
//     uint8_t n = data[0];
//     uint16_t pos = 1;
//     for (uint8_t i = 0; i < n && iBuffer_count < IBUF_SIZE; i++)
//     {
//         if (pos + 9 > datalen)
//             break;
//         SharedState_item_t item;
//         memcpy(&item.id, data + pos, 4);
//         pos += 4;
//         memcpy(&item.ts, data + pos, 4);
//         pos += 4;
//         item.len = data[pos++];
//         if (item.len > sizeof(item.data))
//             item.len = sizeof(item.data);
//         if (pos + item.len > datalen)
//             break;
//         memcpy(item.data, data + pos, item.len);
//         pos += item.len;

//         if (find_in_cache(item.id) < 0)
//         {
//             int already = 0;
//             for (int j = 0; j < iBuffer_count; j++)
//                 if (iBuffer[j].id == item.id)
//                 {
//                     already = 1;
//                     break;
//                 }
//             if (!already)
//                 iBuffer[iBuffer_count++] = item;
//         }
//     }
// }

/* Simple djb2 hash function for strings */
unsigned long hash(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

void sharedState_put(const char *key, const char *value)
{
    LOG_INFO("SharedState_put key: '%s' value: '%s'\n", key, value);
    SharedState_item_t item;
    item.id = hash(key);
    item.ts = clock_seconds();
    item.len = strlen(value);
    memcpy(item.data, value, item.len + 1);
    cache_insert(&item);
}

const char *sharedState_get(const char *key)
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