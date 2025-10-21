#include "contiki.h"
#include "dgmac/dgmac-conf.h"
#include "dgmac/dgmac.h"
//#include "os/net/mac/tsch/tsch.h"
#include "sys/log.h"
//#include <cstdint>

#define SEND_INTERVAL (8 * CLOCK_SECOND)

#define LOG_MODULE "Example"
#define LOG_LEVEL LOG_LEVEL_DBG

static void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  if(len == sizeof(unsigned)) {
    unsigned count = *(unsigned*)data;
    LOG_INFO("Received %u from ", count);
    LOG_INFO_LLADDR(src);
    LOG_INFO_("\n");
  } else {
    LOG_INFO("Received %s\n", (char*)data);
  }
}

PROCESS(dgmac_example_process, "DGMAC SharedState example");
AUTOSTART_PROCESSES(&dgmac_example_process);
PROCESS_THREAD(dgmac_example_process, ev, data)
{
  static struct etimer periodic_timer, time_frame_timer;
  static unsigned count = 0;

  static uint16_t tx = 0;

  PROCESS_BEGIN();

  /* Initialize DGMAC */
  dgmac_init();

  dgmac_set_input_callback(input_callback);

  etimer_set(&periodic_timer, DGMAC_HEARTBEAT_INTERVAL);
  //etimer_set(&time_frame_timer, SEND_INTERVAL);
  while(1) {
    
    tx = get_waiting_time();

    if(tx != 0){
      etimer_set(&time_frame_timer, (DGMAC_HEARTBEAT_INTERVAL/DGMAC_SLOTFRAME_TIMESLOTS) * tx);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&time_frame_timer));
      LOG_INFO("Sending %u\n", count);
      LOG_INFO("Sending in timeframe\n");
      dgmac_broadcast(&count, 1);

      // char msg[] = "Doxxing in progress...\n";

      // dgmac_broadcast(msg, (uint16_t)sizeof(msg));
      count++;
    }
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}
