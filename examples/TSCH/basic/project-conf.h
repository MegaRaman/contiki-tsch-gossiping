#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* -------------------------------------------------------------
 * TSCH configuration
 * ------------------------------------------------------------- */

/* Required for TSCH: 802.15.4e (2015) frame version */
#define FRAME802154_CONF_VERSION FRAME802154_IEEE802154_2015

/* PAN ID used by all nodes */
#define IEEE802154_CONF_PANID 0x1234

/* Automatically start TSCH at boot */
#define TSCH_CONF_AUTOSTART 0

/* Use a simple 1-channel hopping sequence */
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE TSCH_HOPPING_SEQUENCE_1_1

/* Allow unsecured EBs (for simple experiments) */
#define TSCH_CONF_JOIN_SECURED_ONLY 0

/* EB (Enhanced Beacon) period */
#define TSCH_CONF_WITH_EB 1      // coordinator sends EB
#define TSCH_CONF_EB_PERIOD (2 * CLOCK_SECOND)  // EB interval in slots

/* Logging */
/* Use the default log level on Z1 to save ROM. */
// #ifndef CONTIKI_TARGET_Z1
// #define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_WARN
// #define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_WARN
// #define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_WARN
// #define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_WARN
// #define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_INFO
// /* Do not enable LOG_CONF_LEVEL_FRAMER on SimpleLink,
//    that will cause it to print from an interrupt context. */
// #ifndef CONTIKI_TARGET_SIMPLELINK
// #define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_WARN
// #define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_INFO
// #endif
// #endif

// #define TSCH_LOG_CONF_PER_SLOT                     1
#define TSCH_LOG_LEVEL 0  

#endif /* PROJECT_CONF_H_ */