#ifndef I2C_LL_DRIVER_H
#define I2C_LL_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_ll_i2c.h"
#include <stdint.h>

typedef enum
{
  I2C_LL_OK = 0,
  I2C_LL_ERROR,
  I2C_LL_TIMEOUT,
  I2C_LL_BUSY,
  I2C_LL_NACK
} I2C_LL_Status;

typedef enum
{
  I2C_LL_MODE_POLLING = 0,
  I2C_LL_MODE_INTERRUPT
} I2C_LL_Mode;

typedef enum
{
  I2C_LL_STATE_READY = 0,
  I2C_LL_STATE_BUSY,
  I2C_LL_STATE_ERROR
} I2C_LL_State;

typedef struct
{
  I2C_TypeDef *I2Cx;            /* Какой I2C использовать: I2C1, I2C2 или I2C3 */
  uint32_t clock_speed;         /* Скорость I2C в Гц: например 100000 = 100 kHz */
  uint32_t timeout;             /* Таймаут ожидания флагов для polling-режима */
  I2C_LL_Mode mode;             /* Режим передачи: polling или interrupt */

  uint8_t dev_addr;             /* 7-битный адрес устройства для текущей передачи */
  const uint8_t *tx_buffer;     /* Буфер, который передается в interrupt-режиме */
  uint16_t tx_size;             /* Сколько байтов нужно передать */
  volatile uint16_t tx_index;   /* Сколько байтов уже передано */
  volatile uint8_t busy;        /* 1 — I2C занят передачей, 0 — свободен */
  volatile I2C_LL_State state;  /* Состояние I2C-драйвера */
  volatile I2C_LL_Status status;/* Последний статус передачи */
} I2C_LL_Handle;

I2C_LL_Status I2C_LL_Init(I2C_LL_Handle *hi2c);
I2C_LL_Status I2C_LL_Write(I2C_LL_Handle *hi2c, uint8_t dev_addr, const uint8_t *data, uint16_t size);
I2C_LL_Status I2C_LL_Recover(I2C_LL_Handle *hi2c);
uint8_t I2C_LL_IsBusy(I2C_LL_Handle *hi2c);
I2C_LL_Status I2C_LL_GetStatus(I2C_LL_Handle *hi2c);
void I2C_LL_EV_IRQHandler(I2C_TypeDef *I2Cx);
void I2C_LL_ER_IRQHandler(I2C_TypeDef *I2Cx);

#ifdef __cplusplus
}
#endif

#endif /* I2C_LL_DRIVER_H */
