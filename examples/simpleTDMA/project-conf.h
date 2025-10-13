#ifndef PROJECT_CONF_H
#define PROJECT_CONF_H

#define NETSTACK_CONF_NETWORK nullnet_driver
#define NETSTACK_CONF_MAC nullmac_driver
#define NETSTACK_CONF_RDC nullrdc_driver
#define UIP_CONF_IPV6 0

/* Distributed TDMA Configuration */
#define TDMA_SLOT_DURATION      (2 * CLOCK_SECOND)
#define TDMA_SLOTS_PER_FRAME    8                   // More slots for multiple coordinators
#define TDMA_BEACON_INTERVAL    (10 * CLOCK_SECOND)
#define TDMA_NEIGHBOR_TIMEOUT   (30 * CLOCK_SECOND)
#define TDMA_MAX_COORDINATORS   3                   // Max coordinators in network
#define TDMA_HIDDEN_TERMINAL_MARGIN 2               // Extra slots for hidden terminals

/* Increase packet buffer size */
#undef PACKETBUF_CONF_SIZE
#define PACKETBUF_CONF_SIZE 128

#undef CC2420_CONF_AUTOACK
#define CC2420_CONF_AUTOACK 0

#endif