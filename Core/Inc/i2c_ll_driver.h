#ifndef I2C_LL_DRIVER_H
#define I2C_LL_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_ll_i2c.h"
#include <stdint.h>

/*
 * Файл i2c_ll_driver.h
 * ---------------------
 * Это заголовочный файл нижнего уровня I2C-драйвера.
 *
 * Его задача:
 * 1) хранить типы данных для работы с I2C;
 * 2) дать пользователю возможность выбрать I2C1/I2C2/I2C3;
 * 3) поддерживать два режима передачи:
 *    - polling: программа ждёт флаги I2C;
 *    - interrupt: байты отправляются через прерывания I2C;
 * 4) скрыть регистровую работу I2C от верхнего OLED-драйвера.
 *
 * Важно:
 * SSD1306-драйвер не работает напрямую с регистрами I2C.
 * Он вызывает только функции этого файла, например I2C_LL_Write().
 */

/* Результат выполнения функций I2C-драйвера */
typedef enum
{
  I2C_LL_OK = 0,      /* Всё выполнено успешно */
  I2C_LL_ERROR,       /* Общая ошибка I2C */
  I2C_LL_TIMEOUT,     /* Флаг слишком долго не появился */
  I2C_LL_BUSY,        /* I2C занят передачей */
  I2C_LL_NACK         /* Slave-устройство не ответило ACK */
} I2C_LL_Status;

/* Режим работы I2C-передачи */
typedef enum
{
  I2C_LL_MODE_POLLING = 0,  /* Блокирующий режим: ждём флаги в цикле */
  I2C_LL_MODE_INTERRUPT     /* Неблокирующий режим: передача через IRQ */
} I2C_LL_Mode;

/* Состояние I2C-драйвера */
typedef enum
{
  I2C_LL_STATE_READY = 0,   /* I2C свободен */
  I2C_LL_STATE_BUSY,        /* I2C сейчас передаёт данные */
  I2C_LL_STATE_ERROR        /* Произошла ошибка */
} I2C_LL_State;

/*
 * Основная структура I2C-драйвера.
 * Один объект этой структуры соответствует одному выбранному I2C.
 *
 * Пример:
 * I2C_LL_Handle hi2c_oled;
 * hi2c_oled.I2Cx = I2C1;
 */
typedef struct
{
  I2C_TypeDef *I2Cx;            /* Какой I2C использовать: I2C1, I2C2 или I2C3 */
  uint32_t clock_speed;         /* Скорость I2C в Гц: например 100000 = 100 kHz */
  uint32_t timeout;             /* Таймаут ожидания флагов для polling-режима */
  I2C_LL_Mode mode;             /* Режим передачи: polling или interrupt */

  /* Поля ниже в основном нужны для interrupt-режима */
  uint8_t dev_addr;             /* 7-битный адрес устройства для текущей передачи */
  const uint8_t *tx_buffer;     /* Буфер, который передается в interrupt-режиме */
  uint16_t tx_size;             /* Сколько байтов нужно передать */
  volatile uint16_t tx_index;   /* Сколько байтов уже передано */
  volatile uint8_t busy;        /* 1 — I2C занят передачей, 0 — свободен */
  volatile I2C_LL_State state;  /* Состояние I2C-драйвера */
  volatile I2C_LL_Status status;/* Последний статус передачи */
} I2C_LL_Handle;

/* Настройка выбранного I2C */
I2C_LL_Status I2C_LL_Init(I2C_LL_Handle *hi2c);

/*
 * Главная функция передачи данных.
 * Внутри она сама выбирает polling или interrupt по полю hi2c->mode.
 */
I2C_LL_Status I2C_LL_Write(I2C_LL_Handle *hi2c, uint8_t dev_addr, const uint8_t *data, uint16_t size);

/* Попытка восстановить I2C после ошибки */
I2C_LL_Status I2C_LL_Recover(I2C_LL_Handle *hi2c);

/* Проверить, занята ли сейчас interrupt-передача */
uint8_t I2C_LL_IsBusy(I2C_LL_Handle *hi2c);

/* Получить последний статус I2C-драйвера */
I2C_LL_Status I2C_LL_GetStatus(I2C_LL_Handle *hi2c);

/* Обработчик событий I2C: SB, ADDR, TXE, BTF */
void I2C_LL_EV_IRQHandler(I2C_TypeDef *I2Cx);

/* Обработчик ошибок I2C: AF, BERR, ARLO, OVR, TIMEOUT */
void I2C_LL_ER_IRQHandler(I2C_TypeDef *I2Cx);

#ifdef __cplusplus
}
#endif

#endif /* I2C_LL_DRIVER_H */
