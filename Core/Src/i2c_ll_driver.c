#include "i2c_ll_driver.h"
#include "stm32f4xx_ll_bus.h"

/* Таймаут по умолчанию, если пользователь не задал свой timeout */
#define I2C_LL_DEFAULT_TIMEOUT 100000U

/*
 * Включение тактирования выбранного I2C.
 * Без включенного clock модуль I2C работать не будет.
 */
static void I2C_LL_EnableClock(I2C_TypeDef *I2Cx)
{
  if (I2Cx == I2C1)
  {
    /* Включаем тактирование I2C1 на шине APB1 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
  }
  else if (I2Cx == I2C2)
  {
    /* Включаем тактирование I2C2 на шине APB1 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);
  }
  else if (I2Cx == I2C3)
  {
    /* Включаем тактирование I2C3 на шине APB1 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C3);
  }
}

/*
 * Очистка флагов ошибок I2C.
 * Это нужно перед новой передачей или после сбоя.
 */
static void I2C_LL_ClearErrors(I2C_TypeDef *I2Cx)
{
  /* AF = Acknowledge Failure: устройство не ответило ACK */
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
  }

  /* BERR = Bus Error: ошибка на шине I2C */
  if (LL_I2C_IsActiveFlag_BERR(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_BERR(I2Cx);
  }

  /* ARLO = Arbitration Lost: потеря арбитража */
  if (LL_I2C_IsActiveFlag_ARLO(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_ARLO(I2Cx);
  }

  /* OVR = Overrun/Underrun: ошибка переполнения или недочитывания */
  if (LL_I2C_IsActiveFlag_OVR(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_OVR(I2Cx);
  }

  /* TIMEOUT = ошибка таймаута SMBus/I2C */
  if (LL_I2C_IsActiveSMBusFlag_TIMEOUT(I2Cx) != 0U)
  {
    LL_I2C_ClearSMBusFlag_TIMEOUT(I2Cx);
  }
}

/*
 * Проверка ошибок во время ожидания или передачи.
 * Возвращает конкретный статус: OK, NACK или ERROR.
 */
static I2C_LL_Status I2C_LL_CheckErrors(I2C_TypeDef *I2Cx)
{
  /* Если устройство не ответило подтверждением ACK */
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
    return I2C_LL_NACK;
  }

  /* Если возникла любая серьезная ошибка шины */
  if ((LL_I2C_IsActiveFlag_BERR(I2Cx) != 0U) ||
      (LL_I2C_IsActiveFlag_ARLO(I2Cx) != 0U) ||
      (LL_I2C_IsActiveFlag_OVR(I2Cx) != 0U) ||
      (LL_I2C_IsActiveSMBusFlag_TIMEOUT(I2Cx) != 0U))
  {
    I2C_LL_ClearErrors(I2Cx);
    return I2C_LL_ERROR;
  }

  /* Ошибок нет */
  return I2C_LL_OK;
}

/*
 * Ожидание появления нужного флага в регистре SR1.
 * Например: SB, ADDR, TXE или BTF.
 */
static I2C_LL_Status I2C_LL_WaitFlag(I2C_TypeDef *I2Cx, uint32_t flag, uint32_t timeout)
{
  I2C_LL_Status status;

  /* Ждем, пока нужный бит flag не станет равен 1 */
  while ((LL_I2C_ReadReg(I2Cx, SR1) & flag) == 0U)
  {
    /* Во время ожидания постоянно проверяем ошибки */
    status = I2C_LL_CheckErrors(I2Cx);
    if (status != I2C_LL_OK)
    {
      return status;
    }

    /* Если таймаут закончился, выходим с ошибкой TIMEOUT */
    if (timeout == 0U)
    {
      return I2C_LL_TIMEOUT;
    }
    timeout--;
  }

  return I2C_LL_OK;
}

/*
 * Ожидание, пока шина I2C освободится.
 * Перед началом новой передачи BUSY должен быть равен 0.
 */
static I2C_LL_Status I2C_LL_WaitBusFree(I2C_TypeDef *I2Cx, uint32_t timeout)
{
  while (LL_I2C_IsActiveFlag_BUSY(I2Cx) != 0U)
  {
    /* Если шина слишком долго занята, возвращаем BUSY */
    if (timeout == 0U)
    {
      return I2C_LL_BUSY;
    }
    timeout--;
  }

  return I2C_LL_OK;
}

/*
 * Инициализация низкоуровневого I2C-драйвера.
 * Здесь выбирается конкретный I2C и включается сам модуль.
 */
I2C_LL_Status I2C_LL_Init(I2C_LL_Handle *hi2c)
{
  /* Проверяем, что структура и выбранный I2C существуют */
  if ((hi2c == 0) || (hi2c->I2Cx == 0))
  {
    return I2C_LL_ERROR;
  }

  /* Если пользователь не задал timeout, ставим значение по умолчанию */
  if (hi2c->timeout == 0U)
  {
    hi2c->timeout = I2C_LL_DEFAULT_TIMEOUT;
  }

  /* Включаем тактирование выбранного I2C */
  I2C_LL_EnableClock(hi2c->I2Cx);

  /* Включаем сам периферийный блок I2C */
  LL_I2C_Enable(hi2c->I2Cx);

  return I2C_LL_OK;
}

/*
 * Передача массива байтов по I2C.
 * dev_addr передается как 7-битный адрес, например 0x3C для SSD1306.
 */
I2C_LL_Status I2C_LL_Write(I2C_LL_Handle *hi2c, uint8_t dev_addr, const uint8_t *data, uint16_t size)
{
  I2C_TypeDef *I2Cx;
  I2C_LL_Status status;
  uint16_t i;

  /* Проверяем входные параметры */
  if ((hi2c == 0) || (hi2c->I2Cx == 0) || (data == 0))
  {
    return I2C_LL_ERROR;
  }

  /* Если размер 0, отправлять нечего */
  if (size == 0U)
  {
    return I2C_LL_OK;
  }

  /* Берем выбранный I2C из handle */
  I2Cx = hi2c->I2Cx;

  /* Ждем, пока шина I2C будет свободна */
  status = I2C_LL_WaitBusFree(I2Cx, hi2c->timeout);
  if (status != I2C_LL_OK)
  {
    return status;
  }

  /* Перед началом передачи очищаем старые ошибки */
  I2C_LL_ClearErrors(I2Cx);

  /* Формируем START condition на шине I2C */
  LL_I2C_GenerateStartCondition(I2Cx);

  /* Ждем флаг SB: START успешно сформирован */
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_SB, hi2c->timeout);
  if (status != I2C_LL_OK)
  {
    LL_I2C_GenerateStopCondition(I2Cx);
    return status;
  }

  /* Отправляем адрес устройства и бит записи.
   * dev_addr << 1 превращает 7-битный адрес в формат для передачи.
   * Например: 0x3C << 1 = 0x78.
   */
  LL_I2C_TransmitData8(I2Cx, (uint8_t)(dev_addr << 1));

  /* Ждем флаг ADDR: устройство ответило на адрес */
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_ADDR, hi2c->timeout);
  if (status != I2C_LL_OK)
  {
    LL_I2C_GenerateStopCondition(I2Cx);
    return status;
  }

  /* Очищаем флаг ADDR после успешной отправки адреса */
  LL_I2C_ClearFlag_ADDR(I2Cx);

  /* Передаем все байты из массива data */
  for (i = 0U; i < size; i++)
  {
    /* Ждем TXE: регистр передачи пустой, можно записать следующий байт */
    status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_TXE, hi2c->timeout);
    if (status != I2C_LL_OK)
    {
      LL_I2C_GenerateStopCondition(I2Cx);
      return status;
    }

    /* Отправляем один байт */
    LL_I2C_TransmitData8(I2Cx, data[i]);
  }

  /* Ждем BTF: последний байт полностью передан */
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_BTF, hi2c->timeout);

  /* Формируем STOP condition — конец передачи */
  LL_I2C_GenerateStopCondition(I2Cx);

  return status;
}

/*
 * Восстановление I2C после ошибки или зависания.
 * Используется, если передача сорвалась или шина зависла.
 */
I2C_LL_Status I2C_LL_Recover(I2C_LL_Handle *hi2c)
{
  volatile uint32_t delay;

  /* Проверяем, что структура и I2C существуют */
  if ((hi2c == 0) || (hi2c->I2Cx == 0))
  {
    return I2C_LL_ERROR;
  }

  /* Пробуем завершить возможную незавершенную передачу */
  LL_I2C_GenerateStopCondition(hi2c->I2Cx);

  /* Очищаем все флаги ошибок */
  I2C_LL_ClearErrors(hi2c->I2Cx);

  /* Перезапускаем модуль I2C */
  LL_I2C_Disable(hi2c->I2Cx);

  /* Небольшая программная задержка */
  for (delay = 0U; delay < 1000U; delay++)
  {
  }

  /* Включаем I2C обратно */
  LL_I2C_Enable(hi2c->I2Cx);

  return I2C_LL_OK;
}
