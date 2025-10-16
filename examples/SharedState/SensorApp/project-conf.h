#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Use NullNet as network stack */
#define NETSTACK_CONF_NETWORK nullnet_driver

/* Optional: set log levels */
#define LOG_CONF_LEVEL_NULLNET LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_INFO

#endif /* PROJECT_CONF_H_ */