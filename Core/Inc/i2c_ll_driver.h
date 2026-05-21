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

typedef struct
{
  I2C_TypeDef *I2Cx;
  uint32_t timeout;
} I2C_LL_Handle;

I2C_LL_Status I2C_LL_Init(I2C_LL_Handle *hi2c);
I2C_LL_Status I2C_LL_Write(I2C_LL_Handle *hi2c, uint8_t dev_addr, const uint8_t *data, uint16_t size);
I2C_LL_Status I2C_LL_Recover(I2C_LL_Handle *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* I2C_LL_DRIVER_H */
