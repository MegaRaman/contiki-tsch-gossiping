#include "contiki.h"
#include "dgmac/dgmac-conf.h"
#include "dgmac/dgmac.h"
#include "sharedstate/sharedstate.h"
//#include "os/net/mac/tsch/tsch.h"
#include "sys/log.h"
//#include <cstdint>
#include "lib/random.h"
#include <string.h>

#define SEND_INTERVAL (8 * CLOCK_SECOND)

#define LOG_MODULE "Example"
#define LOG_LEVEL LOG_LEVEL_DBG

#define NODES_CNT 20

extern sharedstate_t sharedstate;
extern uint8_t msg_id;

PROCESS(dgmac_example_process, "DGMAC SharedState example");
AUTOSTART_PROCESSES(&dgmac_example_process);
PROCESS_THREAD(dgmac_example_process, ev, data)
{
  static struct etimer periodic_timer, time_frame_timer;
  static uint16_t tx = 0;

  static char sens_val[PKT_DATA_SIZE_BYTES];

  PROCESS_BEGIN();

  /* Initialize DGMAC */
  dgmac_init();
  init_sharedstate(&sharedstate, linkaddr_node_addr.u8[0], dgmac_broadcast);

  dgmac_set_input_callback(sharedstate_recv_callback);

  etimer_set(&periodic_timer, DGMAC_HEARTBEAT_INTERVAL);
  //etimer_set(&time_frame_timer, SEND_INTERVAL);
  while(1) {

    tx = get_waiting_time();

    if(tx != 0){
      etimer_set(&time_frame_timer, (DGMAC_HEARTBEAT_INTERVAL/DGMAC_SLOTFRAME_TIMESLOTS) * tx);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&time_frame_timer));

	  uint8_t rx_id = (random_rand () % NODES_CNT) + 1;
	  while (rx_id == sharedstate.node_id) {
		  rx_id = (random_rand () % NODES_CNT) + 1;
	  }
	  LOG_INFO("shst: tx %d %d\n", rx_id, msg_id);
	  // account for \0
	  sharedstate_app_send(&sharedstate, &sens_val, strlen(sens_val) + 1, rx_id);
	  sharedstate_tx(&sharedstate);

    }
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}
