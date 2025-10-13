#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/mac/tsch/tsch.h"
#include "sys/log.h"
#include "node-id.h"

#include "project-conf.h"

#define LOG_MODULE "simpleTSCH"
#define LOG_LEVEL LOG_LEVEL_INFO

PROCESS(tsch_basic_process, "TSCH Basic Example");
AUTOSTART_PROCESSES(&tsch_basic_process);

static void input_callback(
    const void *data, 
    uint16_t len,
    const linkaddr_t *src, 
    const linkaddr_t *dest)
{
  LOG_INFO("Received %u bytes from node %u: '%.*s'\n", len, src->u8[7], len, (char *)data);
}

PROCESS_THREAD(tsch_basic_process, ev, data)
{
    static struct etimer periodic_timer;
    static char msg[32];
    int is_coordinator;

    PROCESS_BEGIN();

    is_coordinator = (node_id == 1);

    if(is_coordinator) {
        LOG_INFO("Node %u is the TSCH coordinator\n", node_id);
        NETSTACK_ROUTING.root_start();
        tsch_set_coordinator(1);
    }  
    NETSTACK_MAC.on();

    LOG_INFO("Starting TSCH Basic Example\n");
    nullnet_set_input_callback(input_callback);

    while(!tsch_is_associated) {
        PROCESS_PAUSE();
        LOG_INFO("Node %u associated: %u \n", node_id, tsch_is_associated);
        etimer_set(&periodic_timer, CLOCK_SECOND/1000);
    }

    LOG_INFO("Node %u associated with TSCH network\n", node_id);

    etimer_set(&periodic_timer, CLOCK_SECOND * 10);

    while (1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
        etimer_reset(&periodic_timer);

        if(!tsch_is_coordinator) {
            snprintf(msg, sizeof(msg), "Hello from node %u", node_id);
            nullnet_buf = (uint8_t *)msg;
            nullnet_len = strlen(msg) + 1;
            LOG_INFO("Sending: '%s'\n", msg);
            NETSTACK_NETWORK.output(NULL);
        }
    }

    PROCESS_END();
}
