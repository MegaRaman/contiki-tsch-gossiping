#include "contiki.h"
#include "sharedstate/sharedstate.h"
#include "sys/log.h"
#include "sys/etimer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_MODULE "SENSOR"
#define LOG_LEVEL LOG_LEVEL_INFO

PROCESS(sensor_process, "Sensor process");
AUTOSTART_PROCESSES(&sensor_process);

PROCESS_THREAD(sensor_process, ev, data)
{
    static struct etimer periodic_timer;
    static int sens_val = 25;

    PROCESS_BEGIN();

    etimer_set(&periodic_timer, CLOCK_SECOND * 10);

    while (1)
    {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

        //---------- update own sensor value
        sens_val += (rand() % 3) - 1;
        char value_str[8];
        char key_str[8];
        sprintf(key_str, "node%d", linkaddr_node_addr.u8[1]);
        sprintf(value_str, "%d", sens_val);
        LOG_INFO("Sensor SS put: %d\n", sens_val);
        sharedState_put(key_str, value_str);

        //---------- periodic shared state maintenance
        sharedState_periodic();

        //---------- read another node sensor value
        char key_get[8];
        sprintf(key_get, "node%d", (linkaddr_node_addr.u8[1] + 1) % 4);
        const char *val = sharedState_get(key_str);
        if (val)
            LOG_INFO("Sensor value: %s\n", val);

        etimer_reset(&periodic_timer);
    }

    PROCESS_END();
}