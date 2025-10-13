#ifndef PROJECT_CONF_H
#define PROJECT_CONF_H

/* Use NullMAC instead of CSMA for better unicast support */
#define NETSTACK_CONF_NETWORK nullnet_driver
#define NETSTACK_CONF_MAC nullmac_driver
#define NETSTACK_CONF_RDC nullrdc_driver

/* Disable IPv6 to reduce overhead */
#define UIP_CONF_IPV6 0

/* Increase packet buffer size */
#undef PACKETBUF_CONF_SIZE
#define PACKETBUF_CONF_SIZE 128

#endif /* PROJECT_CONF_H */