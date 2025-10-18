#ifndef GCHMAC_H_
#define GCHMAC_H_

#include "gchmac-types.h"

void gchmac_init();

void gchmac_set_input_callback(gchmac_input_callback_t cb);

void gchmac_broadcast(void *data, uint16_t len);

#endif
