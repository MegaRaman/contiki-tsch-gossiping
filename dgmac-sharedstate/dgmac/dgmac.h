#ifndef DGMAC_H_
#define DGMAC_H_

#include "dgmac-types.h"
#include "dgmac-conf.h"
//#include <cstdint>

void dgmac_init();

void dgmac_set_input_callback(dgmac_input_callback_t cb);

void dgmac_broadcast(void *data, uint16_t len);

uint16_t get_waiting_time();

#endif
