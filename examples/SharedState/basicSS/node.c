#include "contiki.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/etimer.h"
#include "sys/log.h"
#include "random.h"
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

typedef struct
{
    uint32_t id;
    uint32_t ts;
    uint8_t len;
    uint8_t data[16];
} SharedState_item_t;

static SharedState_item_t cache[CACHE_SIZE];
static int cache_count;

static SharedState_item_t iBuffer[IBUF_SIZE];
static int iBuffer_count;

static SharedState_item_t oBuffer[OBUF_SIZE];
static int oBuffer_count;

static struct simple_udp_connection udp_conn;

static int find_in_cache(uint32_t id)
{
    for (int i = 0; i < cache_count; i++)
        if (cache[i].id == id)
            return i;
    return -1;
}

static void cache_insert(SharedState_item_t *item)
{
    if (find_in_cache(item->id) >= 0)
        return;
    if (cache_count < CACHE_SIZE)
    {
        cache[cache_count++] = *item;
    }
    else
    {
        int r = random_rand() % CACHE_SIZE;
        cache[r] = *item;
    }
}

static void udp_rx_callback(struct simple_udp_connection *c,
                            const uip_ipaddr_t *sender_addr,
                            uint16_t sender_port,
                            const uip_ipaddr_t *receiver_addr,
                            uint16_t receiver_port,
                            const uint8_t *data,
                            uint16_t datalen)
{
    LOG_INFO("Rx sz: '%d'. from ", datalen);
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");

    if (datalen == 0)
        return;
    uint8_t n = data[0];
    uint16_t pos = 1;
    for (uint8_t i = 0; i < n && iBuffer_count < IBUF_SIZE; i++)
    {
        if (pos + 9 > datalen)
            break;
        SharedState_item_t item;
        memcpy(&item.id, data + pos, 4);
        pos += 4;
        memcpy(&item.ts, data + pos, 4);
        pos += 4;
        item.len = data[pos++];
        if (item.len > sizeof(item.data))
            item.len = sizeof(item.data);
        if (pos + item.len > datalen)
            break;
        memcpy(item.data, data + pos, item.len);
        pos += item.len;

        if (find_in_cache(item.id) < 0)
        {
            int already = 0;
            for (int j = 0; j < iBuffer_count; j++)
                if (iBuffer[j].id == item.id)
                {
                    already = 1;
                    break;
                }
            if (!already)
                iBuffer[iBuffer_count++] = item;
        }
    }
}

/* Simple djb2 hash function for strings */
// unsigned long hash(const char *str)
// {
//     unsigned long hash = 5381;
//     int c;
//     while ((c = *str++))
//         hash = ((hash << 5) + hash) + c;
//     return hash;
// }

// void sharedState_put(const char *key, const char *value)
// {
//     SharedState_item_t item;
//     item.id = hash(key);
//     item.ts = clock_seconds();
//     item.len = strlen(value);
//     memcpy(item.data, value, item.len + 1);
//     cache_insert(&item);
// }

// const char *sharedState_get(const char *key)
// {
//     uint32_t id = hash(key);
//     int idx = find_in_cache(id);
//     if (idx >= 0)
//         return (char *)cache[idx].data;
//     return NULL;
// }

PROCESS(sharedstate_process, "SharedState IPv6 process");
AUTOSTART_PROCESSES(&sharedstate_process);

PROCESS_THREAD(sharedstate_process, ev, data)
{
    static struct etimer round_timer;
    static uip_ipaddr_t mcast_addr;

    PROCESS_BEGIN();

    /* setup UDP multicast */
    simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);
    uip_create_linklocal_allnodes_mcast(&mcast_addr); // sus // other way to use this

    cache_count = iBuffer_count = oBuffer_count = 0;

    /* create one initial item per node */
    {
        SharedState_item_t init;
        init.id = linkaddr_node_addr.u8[0] << 8 | linkaddr_node_addr.u8[1];
        init.ts = clock_seconds();
        init.len = snprintf((char *)init.data, sizeof(init.data),
                            "node%02d", linkaddr_node_addr.u8[0]);
        cache_insert(&init);
    }

    etimer_set(&round_timer, ROUND_INTERVAL);

    while (1)
    {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));

        /* merge input buffer into cache */
        for (int i = 0; i < iBuffer_count; i++)
        {
            cache_insert(&iBuffer[i]);
        }
        iBuffer_count = 0;

        /* select some items to broadcast */
        oBuffer_count = 0;
        for (int i = 0; i < OBUF_SIZE && i < cache_count; i++)
        {
            int idx = random_rand() % cache_count;
            oBuffer[oBuffer_count++] = cache[idx];
        }

        /* build packet */
        uint8_t pkt[128];
        uint16_t pos = 0;
        pkt[pos++] = oBuffer_count;
        for (int i = 0; i < oBuffer_count; i++)
        {
            memcpy(pkt + pos, &oBuffer[i].id, 4);
            pos += 4;
            memcpy(pkt + pos, &oBuffer[i].ts, 4);
            pos += 4;
            pkt[pos++] = oBuffer[i].len;
            memcpy(pkt + pos, oBuffer[i].data, oBuffer[i].len);
            pos += oBuffer[i].len;
        }

        simple_udp_sendto(&udp_conn, pkt, pos, &mcast_addr);
        LOG_INFO("Node %d broadcasted %d items (cache=%d)\n",
                 linkaddr_node_addr.u8[1], oBuffer_count, cache_count);

        etimer_reset(&round_timer);
    }

    PROCESS_END();
}