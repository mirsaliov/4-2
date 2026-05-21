#include "i2c_ll_driver.h"
#include "stm32f4xx_ll_bus.h"

#define I2C_LL_DEFAULT_TIMEOUT 100000U

static void I2C_LL_EnableClock(I2C_TypeDef *I2Cx)
{
  if (I2Cx == I2C1)
  {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
  }
  else if (I2Cx == I2C2)
  {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);
  }
  else if (I2Cx == I2C3)
  {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C3);
  }
}

static void I2C_LL_ClearErrors(I2C_TypeDef *I2Cx)
{
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
  }
  if (LL_I2C_IsActiveFlag_BERR(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_BERR(I2Cx);
  }
  if (LL_I2C_IsActiveFlag_ARLO(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_ARLO(I2Cx);
  }
  if (LL_I2C_IsActiveFlag_OVR(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_OVR(I2Cx);
  }
  if (LL_I2C_IsActiveSMBusFlag_TIMEOUT(I2Cx) != 0U)
  {
    LL_I2C_ClearSMBusFlag_TIMEOUT(I2Cx);
  }
}

static I2C_LL_Status I2C_LL_CheckErrors(I2C_TypeDef *I2Cx)
{
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
    return I2C_LL_NACK;
  }

  if ((LL_I2C_IsActiveFlag_BERR(I2Cx) != 0U) ||
      (LL_I2C_IsActiveFlag_ARLO(I2Cx) != 0U) ||
      (LL_I2C_IsActiveFlag_OVR(I2Cx) != 0U) ||
      (LL_I2C_IsActiveSMBusFlag_TIMEOUT(I2Cx) != 0U))
  {
    I2C_LL_ClearErrors(I2Cx);
    return I2C_LL_ERROR;
  }

  return I2C_LL_OK;
}

static I2C_LL_Status I2C_LL_WaitFlag(I2C_TypeDef *I2Cx, uint32_t flag, uint32_t timeout)
{
  I2C_LL_Status status;

  while ((LL_I2C_ReadReg(I2Cx, SR1) & flag) == 0U)
  {
    status = I2C_LL_CheckErrors(I2Cx);
    if (status != I2C_LL_OK)
    {
      return status;
    }

    if (timeout == 0U)
    {
      return I2C_LL_TIMEOUT;
    }
    timeout--;
  }

  return I2C_LL_OK;
}

static I2C_LL_Status I2C_LL_WaitBusFree(I2C_TypeDef *I2Cx, uint32_t timeout)
{
  while (LL_I2C_IsActiveFlag_BUSY(I2Cx) != 0U)
  {
    if (timeout == 0U)
    {
      return I2C_LL_BUSY;
    }
    timeout--;
  }

  return I2C_LL_OK;
}

I2C_LL_Status I2C_LL_Init(I2C_LL_Handle *hi2c)
{
  if ((hi2c == 0) || (hi2c->I2Cx == 0))
  {
    return I2C_LL_ERROR;
  }

  if (hi2c->timeout == 0U)
  {
    hi2c->timeout = I2C_LL_DEFAULT_TIMEOUT;
  }

  I2C_LL_EnableClock(hi2c->I2Cx);
  LL_I2C_Enable(hi2c->I2Cx);

  return I2C_LL_OK;
}

I2C_LL_Status I2C_LL_Write(I2C_LL_Handle *hi2c, uint8_t dev_addr, const uint8_t *data, uint16_t size)
{
  I2C_TypeDef *I2Cx;
  I2C_LL_Status status;
  uint16_t i;

  if ((hi2c == 0) || (hi2c->I2Cx == 0) || (data == 0))
  {
    return I2C_LL_ERROR;
  }

  if (size == 0U)
  {
    return I2C_LL_OK;
  }

  I2Cx = hi2c->I2Cx;

  status = I2C_LL_WaitBusFree(I2Cx, hi2c->timeout);
  if (status != I2C_LL_OK)
  {
    return status;
  }

  I2C_LL_ClearErrors(I2Cx);

  LL_I2C_GenerateStartCondition(I2Cx);
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_SB, hi2c->timeout);
  if (status != I2C_LL_OK)
  {
    LL_I2C_GenerateStopCondition(I2Cx);
    return status;
  }

  LL_I2C_TransmitData8(I2Cx, (uint8_t)(dev_addr << 1));
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_ADDR, hi2c->timeout);
  if (status != I2C_LL_OK)
  {
    LL_I2C_GenerateStopCondition(I2Cx);
    return status;
  }

  LL_I2C_ClearFlag_ADDR(I2Cx);

  for (i = 0U; i < size; i++)
  {
    status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_TXE, hi2c->timeout);
    if (status != I2C_LL_OK)
    {
      LL_I2C_GenerateStopCondition(I2Cx);
      return status;
    }

    LL_I2C_TransmitData8(I2Cx, data[i]);
  }

  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_BTF, hi2c->timeout);
  LL_I2C_GenerateStopCondition(I2Cx);

  return status;
}

I2C_LL_Status I2C_LL_Recover(I2C_LL_Handle *hi2c)
{
  volatile uint32_t delay;

  if ((hi2c == 0) || (hi2c->I2Cx == 0))
  {
    return I2C_LL_ERROR;
  }

  LL_I2C_GenerateStopCondition(hi2c->I2Cx);
  I2C_LL_ClearErrors(hi2c->I2Cx);

  LL_I2C_Disable(hi2c->I2Cx);
  for (delay = 0U; delay < 1000U; delay++)
  {
  }
  LL_I2C_Enable(hi2c->I2Cx);

  return I2C_LL_OK;
}
