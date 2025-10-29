#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include <stdint.h>

// Contexto opaco del potenciómetro
typedef struct pot_ctx pot_ctx_t;

pot_ctx_t* pot_create(void);
void pot_destroy(pot_ctx_t* ctx);
uint8_t pot_get_percent(pot_ctx_t* ctx); // 0..100
uint32_t pot_get_voltage_mv(pot_ctx_t* ctx); // mV

#endif // POTENTIOMETER_H
