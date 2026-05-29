#include "i2c_ll_driver.h"
#include "stm32f4xx_ll_bus.h"

/* Таймаут по умолчанию, если пользователь не задал свой timeout */
#define I2C_LL_DEFAULT_TIMEOUT 100000U

/* Скорость I2C по умолчанию: 100 kHz */
#define I2C_LL_DEFAULT_CLOCK_SPEED 100000U

/*
 * Проверка выбранного I2C.
 * Драйвер поддерживает только I2C1, I2C2 и I2C3.
 */
static I2C_LL_Status I2C_LL_CheckInstance(I2C_TypeDef *I2Cx)
{
  if ((I2Cx == I2C1) || (I2Cx == I2C2) || (I2Cx == I2C3))
  {
    return I2C_LL_OK;
  }

  return I2C_LL_ERROR;
}

/*
 * Включение тактирования выбранного I2C.
 * GPIO здесь не настраиваются — они должны быть настроены заранее в MX_GPIO_Init().
 */
static I2C_LL_Status I2C_LL_EnableClock(I2C_TypeDef *I2Cx)
{
  if (I2Cx == I2C1)
  {
    /* Включаем тактирование I2C1 на шине APB1 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
    return I2C_LL_OK;
  }
  else if (I2Cx == I2C2)
  {
    /* Включаем тактирование I2C2 на шине APB1 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);
    return I2C_LL_OK;
  }
  else if (I2Cx == I2C3)
  {
    /* Включаем тактирование I2C3 на шине APB1 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C3);
    return I2C_LL_OK;
  }

  return I2C_LL_ERROR;
}

/*
 * Очистка флагов ошибок I2C.
 * Это нужно перед новой передачей или после сбоя.
 */
static void I2C_LL_ClearErrors(I2C_TypeDef *I2Cx)
{
  /* AF = Acknowledge Failure: устройство не ответило ACK */
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U) /* Проверяем флаг AF: нет подтверждения ACK от устройства */
  {
    LL_I2C_ClearFlag_AF(I2Cx); /* Очищаем флаг AF */
  }

  /* BERR = Bus Error: ошибка на шине I2C */
  if (LL_I2C_IsActiveFlag_BERR(I2Cx) != 0U) /* Проверяем флаг BERR: ошибка START/STOP или состояние шины */
  {
    LL_I2C_ClearFlag_BERR(I2Cx); /* Очищаем флаг BERR */
  }

  /* ARLO = Arbitration Lost: потеря арбитража */
  if (LL_I2C_IsActiveFlag_ARLO(I2Cx) != 0U) /* Проверяем флаг ARLO: I2C потерял управление шиной */
  {
    LL_I2C_ClearFlag_ARLO(I2Cx); /* Очищаем флаг ARLO */
  }

  /* OVR = Overrun/Underrun: ошибка переполнения или недочитывания */
  if (LL_I2C_IsActiveFlag_OVR(I2Cx) != 0U) /* Проверяем флаг OVR: данные не успели передаться/прочитаться */
  {
    LL_I2C_ClearFlag_OVR(I2Cx); /* Очищаем флаг OVR */
  }

  /* TIMEOUT = ошибка таймаута SMBus/I2C */
  if (LL_I2C_IsActiveSMBusFlag_TIMEOUT(I2Cx) != 0U) /* Проверяем флаг TIMEOUT: превышено время ожидания на шине */
  {
    LL_I2C_ClearSMBusFlag_TIMEOUT(I2Cx); /* Очищаем флаг TIMEOUT */
  }
}

/*
 * Проверка ошибок во время ожидания или передачи.
 * Возвращает конкретный статус: OK, NACK или ERROR.
 */
static I2C_LL_Status I2C_LL_CheckErrors(I2C_TypeDef *I2Cx)
{
  /* Если устройство не ответило подтверждением ACK */
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U) /* AF: устройство не подтвердило адрес или байт данных */
  {
    LL_I2C_ClearFlag_AF(I2Cx); /* Очищаем AF, чтобы он не мешал следующей передаче */
    return I2C_LL_NACK;
  }

  /* Если возникла любая серьезная ошибка шины */
  if ((LL_I2C_IsActiveFlag_BERR(I2Cx) != 0U) ||          /* BERR: ошибка на I2C-шине */
      (LL_I2C_IsActiveFlag_ARLO(I2Cx) != 0U) ||          /* ARLO: потеря арбитража */
      (LL_I2C_IsActiveFlag_OVR(I2Cx) != 0U) ||           /* OVR: переполнение/недочитывание данных */
      (LL_I2C_IsActiveSMBusFlag_TIMEOUT(I2Cx) != 0U))    /* TIMEOUT: таймаут шины */
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
  while ((LL_I2C_ReadReg(I2Cx, SR1) & flag) == 0U) /* SR1: регистр состояния I2C, flag — ожидаемый флаг */
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
  while (LL_I2C_IsActiveFlag_BUSY(I2Cx) != 0U) /* BUSY: шина I2C занята передачей */
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
 * GPIO не трогаем: пины должны быть настроены через IOC в MX_GPIO_Init().
 */
I2C_LL_Status I2C_LL_Init(I2C_LL_Handle *hi2c)
{
  LL_I2C_InitTypeDef I2C_InitStruct = {0};

  /* Проверяем, что структура и выбранный I2C существуют */
  if ((hi2c == 0) || (hi2c->I2Cx == 0))
  {
    return I2C_LL_ERROR;
  }

  /* Проверяем, что выбран поддерживаемый модуль: I2C1, I2C2 или I2C3 */
  if (I2C_LL_CheckInstance(hi2c->I2Cx) != I2C_LL_OK)
  {
    return I2C_LL_ERROR;
  }

  /* Если пользователь не задал timeout, ставим значение по умолчанию */
  if (hi2c->timeout == 0U)
  {
    hi2c->timeout = I2C_LL_DEFAULT_TIMEOUT;
  }

  /* Если пользователь не задал скорость, ставим стандартные 100 kHz */
  if (hi2c->clock_speed == 0U)
  {
    hi2c->clock_speed = I2C_LL_DEFAULT_CLOCK_SPEED;
  }

  /* Включаем тактирование выбранного I2C */
  if (I2C_LL_EnableClock(hi2c->I2Cx) != I2C_LL_OK)
  {
    return I2C_LL_ERROR;
  }

  /* Перед настройкой лучше выключить I2C */
  LL_I2C_Disable(hi2c->I2Cx);

  /* Очищаем возможные старые ошибки перед новой настройкой */
  I2C_LL_ClearErrors(hi2c->I2Cx);

  /* Отключаем второй адрес устройства. Для master-режима он не нужен */
  LL_I2C_DisableOwnAddress2(hi2c->I2Cx);

  /* Отключаем General Call. Это общий адрес I2C, в данном проекте не нужен */
  LL_I2C_DisableGeneralCall(hi2c->I2Cx);

  /* Разрешаем Clock Stretching */
  LL_I2C_EnableClockStretching(hi2c->I2Cx);

  /* Настройка параметров I2C внутри драйвера */
  I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
  I2C_InitStruct.ClockSpeed = hi2c->clock_speed;
  I2C_InitStruct.DutyCycle = LL_I2C_DUTYCYCLE_2;
  I2C_InitStruct.OwnAddress1 = 0;
  I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
  I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;

  /* Записываем настройки из структуры в регистры выбранного I2C */
  LL_I2C_Init(hi2c->I2Cx, &I2C_InitStruct);

  /* Второй собственный адрес равен 0 и не используется */
  LL_I2C_SetOwnAddress2(hi2c->I2Cx, 0);

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

  /* Проверяем, что выбран поддерживаемый I2C */
  if (I2C_LL_CheckInstance(hi2c->I2Cx) != I2C_LL_OK)
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
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_SB, hi2c->timeout); /* SB = Start Bit */
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
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_ADDR, hi2c->timeout); /* ADDR = Address Sent/Matched */
  if (status != I2C_LL_OK)
  {
    LL_I2C_GenerateStopCondition(I2Cx);
    return status;
  }

  /* Очищаем флаг ADDR после успешной отправки адреса */
  LL_I2C_ClearFlag_ADDR(I2Cx); /* ADDR: флаг успешной передачи адреса */

  /* Передаем все байты из массива data */
  for (i = 0U; i < size; i++)
  {
    /* Ждем TXE: регистр передачи пустой, можно записать следующий байт */
    status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_TXE, hi2c->timeout); /* TXE = Transmit Data Register Empty */
    if (status != I2C_LL_OK)
    {
      LL_I2C_GenerateStopCondition(I2Cx);
      return status;
    }

    /* Отправляем один байт */
    LL_I2C_TransmitData8(I2Cx, data[i]);
  }

  /* Ждем BTF: последний байт полностью передан */
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_BTF, hi2c->timeout); /* BTF = Byte Transfer Finished */

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

  /* Проверяем, что выбран поддерживаемый I2C */
  if (I2C_LL_CheckInstance(hi2c->I2Cx) != I2C_LL_OK)
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
