#include "contiki.h"
// #include "dev/radio.h"
#include "net/netstack.h"
#include <stdio.h>

PROCESS(dummy_process, "Dummy 15.4");
AUTOSTART_PROCESSES(&dummy_process);

// Define a static payload (doesn't matter what it is)
static char payload[] = "PCKT_ABC123DOREMI"; 
static struct etimer et;

PROCESS_THREAD(dummy_process, ev, data)
{
  PROCESS_BEGIN();

  NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, 18);

  printf("Dummy node ON.\n");
  etimer_set(&et, CLOCK_SECOND/2);
  
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    
    NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, 0);
    NETSTACK_RADIO.send((void *)payload, sizeof(payload) - 1);
    printf("Interference transmitted \n");

    etimer_reset(&et);
  }

  PROCESS_END();
}