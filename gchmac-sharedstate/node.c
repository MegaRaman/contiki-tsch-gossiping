#include "contiki.h"
#include "gchmac/gchmac.h"
#include "os/net/mac/tsch/tsch.h"
#include "sys/log.h"

#define SEND_INTERVAL (8 * CLOCK_SECOND)

#define LOG_MODULE "Example"
#define LOG_LEVEL LOG_LEVEL_DBG

void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  if(len == sizeof(unsigned)) {
    unsigned count = *(unsigned*)data;
    LOG_INFO("Received %u from ", count);
    LOG_INFO_LLADDR(src);
    LOG_INFO_("\n");
  }
}

PROCESS(gchmac_example_process, "GCHMAC SharedState example");
AUTOSTART_PROCESSES(&gchmac_example_process);
PROCESS_THREAD(gchmac_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static unsigned count = 0;

  PROCESS_BEGIN();

  /* Initialize GCHMAC */
  gchmac_init();

  gchmac_set_input_callback(input_callback);

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    LOG_INFO("Sending %u\n", count);

    gchmac_broadcast(&count, 1);

    count++;
    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}
