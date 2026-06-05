#include "i2c_ll_driver.h"
#include "stm32f4xx_ll_bus.h"

/*
 * Файл i2c_ll_driver.c
 * --------------------
 * Это нижний уровень драйвера I2C для STM32F407.
 *
 * Этот файл не знает ничего про OLED, SSD1306, картинки или команды дисплея.
 * Его задача только одна: передать массив байтов по выбранному I2C.
 *
 * Драйвер поддерживает два режима:
 *
 * 1) POLLING
 *    Функция I2C_LL_WritePolling() сама ждёт флаги I2C:
 *    BUSY, SB, ADDR, TXE, BTF.
 *    Пока передача не закончится, программа находится внутри этой функции.
 *
 * 2) INTERRUPT
 *    Функция I2C_LL_WriteIT() только запускает передачу:
 *    сохраняет адрес, указатель на буфер, размер буфера и формирует START.
 *    После этого функция сразу выходит, а байты отправляются в прерываниях I2C.
 *
 * Общая схема interrupt-передачи:
 *
 * I2C_LL_WriteIT()
 *   -> сохранить dev_addr, tx_buffer, tx_size
 *   -> busy = 1
 *   -> включить I2C interrupt
 *   -> сгенерировать START
 *   -> выйти
 *
 * I2C_LL_EV_IRQHandler()
 *   SB   -> отправить адрес slave
 *   ADDR -> очистить ADDR чтением SR1, потом SR2
 *   TXE  -> отправить следующий байт из tx_buffer
 *   BTF  -> сформировать STOP, busy = 0
 *
 * I2C_LL_ER_IRQHandler()
 *   AF/BERR/ARLO/OVR/TIMEOUT -> остановить передачу и выставить ошибку
 */

/* Таймаут по умолчанию, если пользователь не задал свой timeout */
#define I2C_LL_DEFAULT_TIMEOUT 100000U

/* Скорость I2C по умолчанию: 100 kHz */
#define I2C_LL_DEFAULT_CLOCK_SPEED 100000U

/*
 * Эти указатели нужны для interrupt-режима.
 *
 * В обычный обработчик прерывания STM32 приходит только номер периферии:
 * I2C1, I2C2 или I2C3.
 *
 * Но внутри драйвера нам нужна структура I2C_LL_Handle,
 * потому что именно в ней хранятся tx_buffer, tx_size, tx_index, busy и status.
 *
 * Поэтому при инициализации мы сохраняем адрес handle в один из этих указателей.
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

/*
 * Получить handle по I2C1/I2C2/I2C3.
 * Эта функция используется внутри обработчиков прерываний.
 */
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
 * Включение прерываний I2C в контроллере NVIC.
 *
 * EV interrupt — обычные события I2C:
 * SB, ADDR, TXE, BTF.
 *
 * ER interrupt — ошибки I2C:
 * AF, BERR, ARLO, OVR, TIMEOUT.
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
 *
 * Важно: NVIC может быть включён, но если внутри I2C выключены EVT/BUF/ERR,
 * то периферия не будет генерировать эти прерывания.
 */
static void I2C_LL_DisableIT(I2C_TypeDef *I2Cx)
{
  LL_I2C_DisableIT_EVT(I2Cx);
  LL_I2C_DisableIT_BUF(I2Cx);
  LL_I2C_DisableIT_ERR(I2Cx);
}

/*
 * Включить источники прерываний внутри I2C.
 * EVT — события I2C.
 * BUF — события буфера, например TXE.
 * ERR — ошибки I2C.
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
  /* AF = Acknowledge Failure: slave не ответил ACK */
  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
  }

  /* BERR = Bus Error: ошибка START/STOP или неправильное состояние шины */
  if (LL_I2C_IsActiveFlag_BERR(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_BERR(I2Cx);
  }

  /* ARLO = Arbitration Lost: потеря управления шиной */
  if (LL_I2C_IsActiveFlag_ARLO(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_ARLO(I2Cx);
  }

  /* OVR = Overrun/Underrun: переполнение или ошибка обмена */
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

/*
 * Ожидание нужного флага в polling-режиме.
 *
 * flag — это бит из регистра SR1, например:
 * LL_I2C_SR1_SB, LL_I2C_SR1_ADDR, LL_I2C_SR1_TXE, LL_I2C_SR1_BTF.
 */
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
 *
 * Это важный нюанс STM32F4:
 * флаг ADDR нельзя очистить простой записью 0.
 * Его нужно очистить чтением SR1, а затем чтением SR2.
 */
static void I2C_LL_ClearADDR(I2C_TypeDef *I2Cx)
{
  volatile uint32_t temp;

  temp = I2Cx->SR1;
  temp = I2Cx->SR2;
  (void)temp;
}

/*
 * Инициализация выбранного I2C.
 *
 * Эта функция вызывается один раз из SSD1306_Begin().
 * Здесь настраивается сама периферия I2C и сбрасываются служебные поля handle.
 */
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

  hi2c->dev_addr = 0U;
  hi2c->tx_buffer = 0;
  hi2c->tx_size = 0U;
  hi2c->tx_index = 0U;
  hi2c->busy = 0U;
  hi2c->state = I2C_LL_STATE_READY;
  hi2c->status = I2C_LL_OK;

  LL_I2C_Enable(hi2c->I2Cx);

  if (hi2c->mode == I2C_LL_MODE_INTERRUPT)
  {
    I2C_LL_EnableIRQ(hi2c->I2Cx);
  }

  return I2C_LL_OK;
}

/*
 * Блокирующая передача I2C.
 *
 * Последовательность:
 * 1) ждём, пока шина свободна;
 * 2) формируем START;
 * 3) ждём SB;
 * 4) отправляем адрес slave;
 * 5) ждём ADDR;
 * 6) очищаем ADDR чтением SR1/SR2;
 * 7) по TXE отправляем каждый байт;
 * 8) ждём BTF;
 * 9) формируем STOP.
 */
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

  I2C_LL_ClearADDR(I2Cx);

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

/*
 * Неблокирующий запуск передачи.
 *
 * Эта функция НЕ отправляет все байты сама.
 * Она только подготавливает данные для IRQ и формирует START.
 * Дальше байты отправляются в I2C_LL_EV_IRQHandler().
 */
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

  if (hi2c->busy != 0U)
  {
    return I2C_LL_BUSY;
  }

  if (LL_I2C_IsActiveFlag_BUSY(hi2c->I2Cx) != 0U)
  {
    return I2C_LL_BUSY;
  }

  I2C_LL_ClearErrors(hi2c->I2Cx);

  hi2c->dev_addr = dev_addr;
  hi2c->tx_buffer = data;
  hi2c->tx_size = size;
  hi2c->tx_index = 0U;
  hi2c->busy = 1U;
  hi2c->state = I2C_LL_STATE_BUSY;
  hi2c->status = I2C_LL_BUSY;

  I2C_LL_EnableIRQ(hi2c->I2Cx);
  I2C_LL_EnableIT(hi2c->I2Cx);

  LL_I2C_GenerateStartCondition(hi2c->I2Cx);

  return I2C_LL_BUSY;
}

/*
 * Общая функция записи.
 * Верхний уровень вызывает только её.
 * А она уже сама выбирает polling или interrupt по hi2c->mode.
 */
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

/*
 * Общий обработчик event interrupt для I2C1/I2C2/I2C3.
 *
 * Одно прерывание — одно маленькое действие:
 * SB   -> отправить адрес;
 * ADDR -> очистить ADDR;
 * TXE  -> отправить один байт;
 * BTF  -> завершить передачу.
 */
void I2C_LL_EV_IRQHandler(I2C_TypeDef *I2Cx)
{
  I2C_LL_Handle *hi2c;

  hi2c = I2C_LL_GetHandle(I2Cx);
  if ((hi2c == 0) || (hi2c->busy == 0U))
  {
    return;
  }

  if (LL_I2C_IsActiveFlag_SB(I2Cx) != 0U)
  {
    LL_I2C_TransmitData8(I2Cx, (uint8_t)(hi2c->dev_addr << 1));
    return;
  }

  if (LL_I2C_IsActiveFlag_ADDR(I2Cx) != 0U)
  {
    I2C_LL_ClearADDR(I2Cx);
    return;
  }

  if ((LL_I2C_IsActiveFlag_TXE(I2Cx) != 0U) && (hi2c->tx_index < hi2c->tx_size))
  {
    LL_I2C_TransmitData8(I2Cx, hi2c->tx_buffer[hi2c->tx_index]);
    hi2c->tx_index++;

    if (hi2c->tx_index >= hi2c->tx_size)
    {
      LL_I2C_DisableIT_BUF(I2Cx);
    }

    return;
  }

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

  if (LL_I2C_IsActiveFlag_AF(I2Cx) != 0U)
  {
    LL_I2C_ClearFlag_AF(I2Cx);
    hi2c->status = I2C_LL_NACK;
  }
  else
  {
    hi2c->status = I2C_LL_ERROR;
  }

  I2C_LL_ClearErrors(I2Cx);
  LL_I2C_GenerateStopCondition(I2Cx);
  I2C_LL_DisableIT(I2Cx);

  hi2c->busy = 0U;
  hi2c->state = I2C_LL_STATE_ERROR;
}

/*
 * Попытка восстановить I2C после ошибки.
 *
 * Функция формирует STOP, очищает ошибки, отключает I2C,
 * делает небольшую паузу и включает I2C обратно.
 */
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
