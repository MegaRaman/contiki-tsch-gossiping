#ifndef SHAREDSTATE_H_
#define SHAREDSTATE_H_

#define SHAREDSTATE_MAX_ITEMS 32
#define SHAREDSTATE_MAX_DATA_LEN 16

#define NETSTACK_CONF_NETWORK nullnet_driver
#define NETSTACK_CONF_WITH_NULLNET 1

#define LOG_CONF_LEVEL_NULLNET LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_INFO

typedef struct
{
    uint32_t id;
    uint32_t ts;
    uint8_t len;
    uint8_t data[SHAREDSTATE_MAX_DATA_LEN];
} SharedState_item_t;

void sharedstate_init();
void sharedstate_periodic();
void sharedstate_put(const char *key, const char *value);
const char *sharedstate_get(const char *key);

#endif /* SHAREDSTATE_H_ */