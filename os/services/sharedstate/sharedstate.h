#ifndef SHAREDSTATE_H_
#define SHAREDSTATE_H_

#define SHAREDSTATE_MAX_ITEM = 32
#define SHAREDSTATE_KEY_MAX_LEN = 16

typedef struct
{
    uint32_t id;
    uint32_t ts;
    uint8_t len;
    uint8_t data[16];
} SharedState_item_t;

void sharedState_init();
void sharedState_periodic();
void sharedState_put(const char *key, const char *value);
const char *sharedState_get(const char *key);

#endif /* SHAREDSTATE_H_ */