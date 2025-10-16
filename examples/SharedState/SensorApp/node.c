#include "contiki.h"
#include "sharedstate/sharedstate.h"
#include "sys/log.h"
#include "sys/etimer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_MODULE "SENSOR"
#define LOG_LEVEL LOG_LEVEL_INFO

#define PERCENTAGE_CHANCE_BROADCAST 35

PROCESS(sensor_process, "Sensor process");
AUTOSTART_PROCESSES(&sensor_process);

PROCESS_THREAD(sensor_process, ev, data)
{
    static struct etimer periodic_timer;
    static int sens_val = 25;

    PROCESS_BEGIN();

    sharedstate_init();
    etimer_set(&periodic_timer, CLOCK_SECOND * 10);

    while (1)
    {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

        int rand_num = rand() % PERCENTAGE_CHANCE_BROADCAST;

        if (!rand_num)
        {
            //---------- update own sensor value
            sens_val += (rand() % 3) - 1;
            char value_str[8];
            char key_str[8];
            sprintf(key_str, "node%d", linkaddr_node_addr.u8[1]);
            sprintf(value_str, "%d", sens_val);
            LOG_INFO("Sensor SS put: %d\n", sens_val);
            sharedstate_put(key_str, value_str);

            //---------- periodic shared state maintenance
            sharedstate_periodic();

            //---------- read another node sensor value
            char key_get[8];
            sprintf(key_get, "node%d", (linkaddr_node_addr.u8[1] + 1) % 4);
            const char *val = sharedstate_get(key_get);
            if (val)
                LOG_INFO("Sensor value: %s\n", val);
        }

        etimer_reset(&periodic_timer);
    }

    PROCESS_END();
}