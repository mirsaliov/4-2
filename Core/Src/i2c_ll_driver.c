#include "i2c_ll_driver.h"
#include "stm32f4xx_ll_bus.h"

/* Таймаут по умолчанию, если пользователь не задал свой timeout */
#define I2C_LL_DEFAULT_TIMEOUT 100000U

/* Скорость I2C по умолчанию: 100 kHz */
#define I2C_LL_DEFAULT_CLOCK_SPEED 100000U

/*
 * Эти указатели нужны для interrupt-режима.
 * В IRQ-функцию приходит только I2C1/I2C2/I2C3,
 * а нам нужно найти соответствующую структуру I2C_LL_Handle.
 */
static I2C_LL_Handle *i2c1_handle = 0;
static I2C_LL_Handle *i2c2_handle = 0;
static I2C_LL_Handle *i2c3_handle = 0;

/* Проверка выбранного I2C */
static I2C_LL_Status I2C_LL_CheckInstance(I2C_TypeDef *I2Cx)
{
  if ((I2Cx == I2C1) || (I2Cx == I2C2) || (I2Cx == I2C3))
  {
    return I2C_LL_OK;
  }

  return I2C_LL_ERROR;
}

/* Получить handle по I2C1/I2C2/I2C3. Это нужно для IRQ обработчиков */
static I2C_LL_Handle *I2C_LL_GetHandle(I2C_TypeDef *I2Cx)
{
  if (I2Cx == I2C1)
  {
    return i2c1_handle;
  }
  else if (I2Cx == I2C2)
  {
    return i2c2_handle;
  }
  else if (I2Cx == I2C3)
  {
    return i2c3_handle;
  }

  return 0;
}

/* Запомнить handle для выбранного I2C */
static void I2C_LL_SaveHandle(I2C_LL_Handle *hi2c)
{
  if (hi2c->I2Cx == I2C1)
  {
    i2c1_handle = hi2c;
  }
  else if (hi2c->I2Cx == I2C2)
  {
    i2c2_handle = hi2c;
  }
  else if (hi2c->I2Cx == I2C3)
  {
    i2c3_handle = hi2c;
  }
}

/* Включение тактирования выбранного I2C */
static I2C_LL_Status I2C_LL_EnableClock(I2C_TypeDef *I2Cx)
{
  if (I2Cx == I2C1)
  {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);
    return I2C_LL_OK;
  }
  else if (I2Cx == I2C2)
  {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);
    return I2C_LL_OK;
  }
  else if (I2Cx == I2C3)
  {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C3);
    return I2C_LL_OK;
  }

  return I2C_LL_ERROR;
}

/*
 * Включение прерываний I2C в NVIC.
 * EV interrupt — события I2C: SB, ADDR, TXE, BTF.
 * ER interrupt — ошибки I2C: AF, BERR, ARLO, OVR, TIMEOUT.
 */
static void I2C_LL_EnableIRQ(I2C_TypeDef *I2Cx)
{
  if (I2Cx == I2C1)
  {
    NVIC_SetPriority(I2C1_EV_IRQn, 1U);
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_SetPriority(I2C1_ER_IRQn, 1U);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
  }
  else if (I2Cx == I2C2)
  {
    NVIC_SetPriority(I2C2_EV_IRQn, 1U);
    NVIC_EnableIRQ(I2C2_EV_IRQn);
    NVIC_SetPriority(I2C2_ER_IRQn, 1U);
    NVIC_EnableIRQ(I2C2_ER_IRQn);
  }
  else if (I2Cx == I2C3)
  {
    NVIC_SetPriority(I2C3_EV_IRQn, 1U);
    NVIC_EnableIRQ(I2C3_EV_IRQn);
    NVIC_SetPriority(I2C3_ER_IRQn, 1U);
    NVIC_EnableIRQ(I2C3_ER_IRQn);
  }
}

/*
 * Отключить источники прерываний внутри самого I2C.
 * EVT — события, BUF — пустой/полный буфер, ERR — ошибки.
 */
static void I2C_LL_DisableIT(I2C_TypeDef *I2Cx)
{
  LL_I2C_DisableIT_EVT(I2Cx);
  LL_I2C_DisableIT_BUF(I2Cx);
  LL_I2C_DisableIT_ERR(I2Cx);
}

/*
 * Включить источники прерываний внутри I2C.
 * После этого аппаратный I2C сам будет вызывать IRQ при событиях.
 */
static void I2C_LL_EnableIT(I2C_TypeDef *I2Cx)
{
  LL_I2C_EnableIT_EVT(I2Cx);
  LL_I2C_EnableIT_BUF(I2Cx);
  LL_I2C_EnableIT_ERR(I2Cx);
}

/* Очистка флагов ошибок I2C */
static void I2C_LL_ClearErrors(I2C_TypeDef *I2Cx)
{
  /* AF = Acknowledge Failure: устройство не ответило ACK */
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
  }

  /* BERR = Bus Error: ошибка START/STOP или состояние шины */
  if (LL_I2C_IsActiveFlag_BERR(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_BERR(I2Cx);
  }

  /* ARLO = Arbitration Lost: потеря управления шиной */
  if (LL_I2C_IsActiveFlag_ARLO(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_ARLO(I2Cx);
  }

  /* OVR = Overrun/Underrun: переполнение или недочитывание */
  if (LL_I2C_IsActiveFlag_OVR(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_OVR(I2Cx);
  }

  /* TIMEOUT = таймаут шины I2C/SMBus */
  if (LL_I2C_IsActiveSMBusFlag_TIMEOUT(I2Cx) != 0U)
  {
    LL_I2C_ClearSMBusFlag_TIMEOUT(I2Cx);
  }
}

/* Проверка ошибок в polling-режиме */
static I2C_LL_Status I2C_LL_CheckErrors(I2C_TypeDef *I2Cx)
{
  /* AF: slave не подтвердил адрес или байт данных */
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
    return I2C_LL_NACK;
  }

  /* Остальные ошибки считаем общей ошибкой шины */
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

/* Ожидание флага в polling-режиме */
static I2C_LL_Status I2C_LL_WaitFlag(I2C_TypeDef *I2Cx, uint32_t flag, uint32_t timeout)
{
  I2C_LL_Status status;

  /* Ждем, пока нужный флаг в SR1 станет равен 1 */
  while ((LL_I2C_ReadReg(I2Cx, SR1) & flag) == 0U)
  {
    /* Во время ожидания обязательно проверяем ошибки */
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

/* Ожидание свободной шины в polling-режиме */
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

/*
 * Очистка ADDR для STM32F4.
 * Важно: ADDR очищается только чтением SR1, потом SR2.
 * Если просто пропустить этот момент, I2C может зависнуть на флаге ADDR.
 */
static void I2C_LL_ClearADDR(I2C_TypeDef *I2Cx)
{
  volatile uint32_t temp;

  temp = I2Cx->SR1;
  temp = I2Cx->SR2;
  (void)temp;
}

I2C_LL_Status I2C_LL_Init(I2C_LL_Handle *hi2c)
{
  LL_I2C_InitTypeDef I2C_InitStruct = {0};

  if ((hi2c == 0) || (hi2c->I2Cx == 0))
  {
    return I2C_LL_ERROR;
  }

  if (I2C_LL_CheckInstance(hi2c->I2Cx) != I2C_LL_OK)
  {
    return I2C_LL_ERROR;
  }

  if (hi2c->timeout == 0U)
  {
    hi2c->timeout = I2C_LL_DEFAULT_TIMEOUT;
  }

  if (hi2c->clock_speed == 0U)
  {
    hi2c->clock_speed = I2C_LL_DEFAULT_CLOCK_SPEED;
  }

  if (I2C_LL_EnableClock(hi2c->I2Cx) != I2C_LL_OK)
  {
    return I2C_LL_ERROR;
  }

  /* Сохраняем handle, чтобы потом найти его из IRQ обработчика */
  I2C_LL_SaveHandle(hi2c);

  LL_I2C_Disable(hi2c->I2Cx);
  I2C_LL_ClearErrors(hi2c->I2Cx);
  I2C_LL_DisableIT(hi2c->I2Cx);

  LL_I2C_DisableOwnAddress2(hi2c->I2Cx);
  LL_I2C_DisableGeneralCall(hi2c->I2Cx);
  LL_I2C_EnableClockStretching(hi2c->I2Cx);

  I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
  I2C_InitStruct.ClockSpeed = hi2c->clock_speed;
  I2C_InitStruct.DutyCycle = LL_I2C_DUTYCYCLE_2;
  I2C_InitStruct.OwnAddress1 = 0;
  I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
  I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;

  LL_I2C_Init(hi2c->I2Cx, &I2C_InitStruct);
  LL_I2C_SetOwnAddress2(hi2c->I2Cx, 0);

  /* Сбрасываем служебные поля передачи */
  hi2c->dev_addr = 0U;
  hi2c->tx_buffer = 0;
  hi2c->tx_size = 0U;
  hi2c->tx_index = 0U;
  hi2c->busy = 0U;
  hi2c->state = I2C_LL_STATE_READY;
  hi2c->status = I2C_LL_OK;

  LL_I2C_Enable(hi2c->I2Cx);

  /* Если выбран interrupt-режим, включаем IRQ в NVIC */
  if (hi2c->mode == I2C_LL_MODE_INTERRUPT)
  {
    I2C_LL_EnableIRQ(hi2c->I2Cx);
  }

  return I2C_LL_OK;
}

/* Старый блокирующий режим. Polling оставлен отдельно */
static I2C_LL_Status I2C_LL_WritePolling(I2C_LL_Handle *hi2c, uint8_t dev_addr, const uint8_t *data, uint16_t size)
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

  /* START: начало передачи на шине I2C */
  LL_I2C_GenerateStartCondition(I2Cx);

  /* SB = Start Bit: аппарат подтвердил, что START сформирован */
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_SB, hi2c->timeout);
  if (status != I2C_LL_OK)
  {
    LL_I2C_GenerateStopCondition(I2Cx);
    return status;
  }

  /* Отправляем 7-битный адрес + бит записи */
  LL_I2C_TransmitData8(I2Cx, (uint8_t)(dev_addr << 1));

  /* ADDR = адрес отправлен и подтвержден ACK от slave */
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_ADDR, hi2c->timeout);
  if (status != I2C_LL_OK)
  {
    LL_I2C_GenerateStopCondition(I2Cx);
    return status;
  }

  I2C_LL_ClearADDR(I2Cx);

  for (i = 0U; i < size; i++)
  {
    /* TXE = Data Register Empty: можно записать следующий байт */
    status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_TXE, hi2c->timeout);
    if (status != I2C_LL_OK)
    {
      LL_I2C_GenerateStopCondition(I2Cx);
      return status;
    }

    LL_I2C_TransmitData8(I2Cx, data[i]);
  }

  /* BTF = Byte Transfer Finished: последний байт полностью ушёл */
  status = I2C_LL_WaitFlag(I2Cx, LL_I2C_SR1_BTF, hi2c->timeout);
  LL_I2C_GenerateStopCondition(I2Cx);

  return status;
}

/* Неблокирующий запуск передачи. Сами байты уйдут в I2C interrupt */
static I2C_LL_Status I2C_LL_WriteIT(I2C_LL_Handle *hi2c, uint8_t dev_addr, const uint8_t *data, uint16_t size)
{
  if ((hi2c == 0) || (hi2c->I2Cx == 0) || (data == 0))
  {
    return I2C_LL_ERROR;
  }

  if (size == 0U)
  {
    return I2C_LL_OK;
  }

  /* Если предыдущая interrupt-передача ещё идёт, новую не запускаем */
  if (hi2c->busy != 0U)
  {
    return I2C_LL_BUSY;
  }

  /* Проверяем аппаратный BUSY: шина физически занята */
  if (LL_I2C_IsActiveFlag_BUSY(hi2c->I2Cx) != 0U)
  {
    return I2C_LL_BUSY;
  }

  I2C_LL_ClearErrors(hi2c->I2Cx);

  /* Сохраняем параметры передачи, которые потом будет использовать IRQ */
  hi2c->dev_addr = dev_addr;
  hi2c->tx_buffer = data;
  hi2c->tx_size = size;
  hi2c->tx_index = 0U;
  hi2c->busy = 1U;
  hi2c->state = I2C_LL_STATE_BUSY;
  hi2c->status = I2C_LL_BUSY;

  I2C_LL_EnableIRQ(hi2c->I2Cx);
  I2C_LL_EnableIT(hi2c->I2Cx);

  /* Стартуем передачу. Дальше работа пойдёт в I2C_LL_EV_IRQHandler() */
  LL_I2C_GenerateStartCondition(hi2c->I2Cx);

  /* Возвращаем BUSY: это не ошибка, а признак, что передача запущена */
  return I2C_LL_BUSY;
}

/* Общая функция записи. Она сама выбирает polling или interrupt */
I2C_LL_Status I2C_LL_Write(I2C_LL_Handle *hi2c, uint8_t dev_addr, const uint8_t *data, uint16_t size)
{
  if ((hi2c == 0) || (hi2c->I2Cx == 0) || (data == 0))
  {
    return I2C_LL_ERROR;
  }

  if (I2C_LL_CheckInstance(hi2c->I2Cx) != I2C_LL_OK)
  {
    return I2C_LL_ERROR;
  }

  if (hi2c->mode == I2C_LL_MODE_INTERRUPT)
  {
    return I2C_LL_WriteIT(hi2c, dev_addr, data, size);
  }

  return I2C_LL_WritePolling(hi2c, dev_addr, data, size);
}

uint8_t I2C_LL_IsBusy(I2C_LL_Handle *hi2c)
{
  if (hi2c == 0)
  {
    return 0U;
  }

  return hi2c->busy;
}

I2C_LL_Status I2C_LL_GetStatus(I2C_LL_Handle *hi2c)
{
  if (hi2c == 0)
  {
    return I2C_LL_ERROR;
  }

  return hi2c->status;
}

/* Общий обработчик event interrupt для I2C1/I2C2/I2C3 */
void I2C_LL_EV_IRQHandler(I2C_TypeDef *I2Cx)
{
  I2C_LL_Handle *hi2c;

  hi2c = I2C_LL_GetHandle(I2Cx);
  if ((hi2c == 0) || (hi2c->busy == 0U))
  {
    return;
  }

  /*
   * SB = Start Bit.
   * Этот флаг появляется после формирования START.
   * После него нужно записать адрес slave + бит Write.
   */
  if (LL_I2C_IsActiveFlag_SB(I2Cx) != 0U)
  {
    LL_I2C_TransmitData8(I2Cx, (uint8_t)(hi2c->dev_addr << 1));
    return;
  }

  /*
   * ADDR = Address sent.
   * Для STM32F4 этот флаг очищается только чтением SR1, потом SR2.
   */
  if (LL_I2C_IsActiveFlag_ADDR(I2Cx) != 0U)
  {
    I2C_LL_ClearADDR(I2Cx);
    return;
  }

  /*
   * TXE = Transmit Data Register Empty.
   * DR пустой, значит можно загрузить следующий байт из tx_buffer.
   */
  if ((LL_I2C_IsActiveFlag_TXE(I2Cx) != 0U) && (hi2c->tx_index < hi2c->tx_size))
  {
    LL_I2C_TransmitData8(I2Cx, hi2c->tx_buffer[hi2c->tx_index]);
    hi2c->tx_index++;

    /*
     * Если загрузили последний байт, выключаем IT_BUF.
     * Иначе TXE будет вызывать пустые прерывания, пока не появится BTF.
     * IT_EVT оставляем включённым — он поймает BTF для формирования STOP.
     */
    if (hi2c->tx_index >= hi2c->tx_size)
    {
      LL_I2C_DisableIT_BUF(I2Cx);
    }

    return;
  }

  /*
   * BTF = Byte Transfer Finished.
   * Этот флаг означает, что последний байт не просто записан в DR,
   * а реально полностью передан через сдвиговый регистр I2C.
   * После BTF можно безопасно формировать STOP.
   */
  if ((hi2c->tx_index >= hi2c->tx_size) && (LL_I2C_IsActiveFlag_BTF(I2Cx) != 0U))
  {
    LL_I2C_GenerateStopCondition(I2Cx);
    I2C_LL_DisableIT(I2Cx);

    hi2c->busy = 0U;
    hi2c->state = I2C_LL_STATE_READY;
    hi2c->status = I2C_LL_OK;
    return;
  }
}

/* Общий обработчик error interrupt для I2C1/I2C2/I2C3 */
void I2C_LL_ER_IRQHandler(I2C_TypeDef *I2Cx)
{
  I2C_LL_Handle *hi2c;

  hi2c = I2C_LL_GetHandle(I2Cx);
  if (hi2c == 0)
  {
    I2C_LL_ClearErrors(I2Cx);
    return;
  }

  /* AF = нет ACK от slave. Для OLED это чаще всего неверный адрес/подключение */
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
    hi2c->status = I2C_LL_NACK;
  }
  else
  {
    hi2c->status = I2C_LL_ERROR;
  }

  /* При ошибке останавливаем передачу и освобождаем драйвер */
  I2C_LL_ClearErrors(I2Cx);
  LL_I2C_GenerateStopCondition(I2Cx);
  I2C_LL_DisableIT(I2Cx);

  hi2c->busy = 0U;
  hi2c->state = I2C_LL_STATE_ERROR;
}

I2C_LL_Status I2C_LL_Recover(I2C_LL_Handle *hi2c)
{
  volatile uint32_t delay;

  if ((hi2c == 0) || (hi2c->I2Cx == 0))
  {
    return I2C_LL_ERROR;
  }

  if (I2C_LL_CheckInstance(hi2c->I2Cx) != I2C_LL_OK)
  {
    return I2C_LL_ERROR;
  }

  LL_I2C_GenerateStopCondition(hi2c->I2Cx);
  I2C_LL_ClearErrors(hi2c->I2Cx);
  I2C_LL_DisableIT(hi2c->I2Cx);

  LL_I2C_Disable(hi2c->I2Cx);

  for (delay = 0U; delay < 1000U; delay++)
  {
  }

  hi2c->busy = 0U;
  hi2c->state = I2C_LL_STATE_READY;
  hi2c->status = I2C_LL_OK;

  LL_I2C_Enable(hi2c->I2Cx);

  return I2C_LL_OK;
}
