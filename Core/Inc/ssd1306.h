#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include "i2c_ll_driver.h"
#include <stdint.h>

/* Размер OLED-дисплея SSD1306 */
#define SSD1306_WIDTH 128U
#define SSD1306_HEIGHT 64U

/* Размер видеобуфера: 128 * 64 / 8 = 1024 байта */
#define SSD1306_BUFFER_SIZE ((SSD1306_WIDTH * SSD1306_HEIGHT) / 8U)

/* Буфер для I2C-передачи: 1 control byte 0x40 + 1024 байта картинки */
#define SSD1306_TX_BUFFER_SIZE (SSD1306_BUFFER_SIZE + 1U)

/* Размер очереди пользовательских команд */
#define SSD1306_COMMAND_QUEUE_SIZE 16U

/* Стандартный 7-битный адрес OLED SSD1306 */
#define SSD1306_I2C_ADDR_7BIT 0x3CU

/* Цвета для функций рисования */
#define SSD1306_COLOR_BLACK 0U
#define SSD1306_COLOR_WHITE 1U

/* Общий статус функций SSD1306 */
typedef enum
{
  SSD1306_OK = 0,
  SSD1306_ERROR
} SSD1306_Status;

/* Состояние верхнего обработчика команд */
typedef enum
{
  SSD1306_STATE_WAIT = 0, /* Ждём команду из очереди */
  SSD1306_STATE_WORK,    /* Выполняем текущую команду */
  SSD1306_STATE_ERROR    /* Ошибка драйвера */
} SSD1306_State;

/*
 * Конфигурация дисплея.
 * Пользователь заполняет эту структуру в main.c.
 */
typedef struct
{
  I2C_TypeDef *I2Cx;      /* Какой I2C использовать: I2C1, I2C2 или I2C3 */
  uint32_t clock_speed;   /* Скорость I2C, например 100000 = 100 kHz */
  uint32_t timeout;       /* Таймаут для polling-режима */
  uint8_t address;        /* 7-битный адрес SSD1306, обычно 0x3C */
  I2C_LL_Mode mode;       /* Режим передачи: polling или interrupt */
} SSD1306_Config;

/* Тип пользовательской команды */
typedef enum
{
  SSD1306_CMD_CLEAR = 0,  /* Очистить буфер */
  SSD1306_CMD_FILL,       /* Залить буфер цветом */
  SSD1306_CMD_PIXEL,      /* Нарисовать пиксель */
  SSD1306_CMD_LINE,       /* Нарисовать линию */
  SSD1306_CMD_RECT,       /* Нарисовать прямоугольник */
  SSD1306_CMD_BITMAP,     /* Нарисовать bitmap */
  SSD1306_CMD_UPDATE      /* Отправить buffer[] на OLED */
} SSD1306_CommandType;

/*
 * Одна команда для OLED.
 * Не все поля используются одновременно.
 * Например для LINE нужны x0,y0,x1,y1,color,
 * а для RECT нужны x0,y0,width,height,color.
 */
typedef struct
{
  SSD1306_CommandType type; /* Какую команду надо выполнить */

  int16_t x0;              /* X начальной точки или левого верхнего угла */
  int16_t y0;              /* Y начальной точки или левого верхнего угла */
  int16_t x1;              /* X конечной точки линии */
  int16_t y1;              /* Y конечной точки линии */

  uint8_t width;           /* Ширина для RECT/BITMAP */
  uint8_t height;          /* Высота для RECT/BITMAP */
  uint8_t color;           /* Цвет: BLACK или WHITE */

  const uint8_t *bitmap;   /* Указатель на массив картинки */
} SSD1306_Command;

/*
 * Основная структура дисплея.
 * Здесь хранится состояние OLED, буфер картинки и очередь команд.
 */
typedef struct
{
  I2C_LL_Handle *i2c;                         /* Указатель на нижний I2C-драйвер */
  uint8_t address;                            /* Адрес OLED */
  uint8_t buffer[SSD1306_BUFFER_SIZE];        /* Видеобуфер 1024 байта */
  uint8_t tx_buffer[SSD1306_TX_BUFFER_SIZE];  /* Постоянный буфер для interrupt-передачи */
  uint8_t initialized;                        /* 1 — дисплей инициализирован */

  SSD1306_Command commands[SSD1306_COMMAND_QUEUE_SIZE]; /* Очередь команд */
  uint8_t head;                               /* Индекс текущей команды */
  uint8_t count;                              /* Количество команд в очереди */
  SSD1306_State state;                        /* Состояние Handler */
} SSD1306_Handle;

SSD1306_Status SSD1306_Begin(SSD1306_Handle *display, I2C_LL_Handle *i2c, const SSD1306_Config *config);

void SSD1306_ClearCommands(SSD1306_Handle *display);
SSD1306_Status SSD1306_AddCommand(SSD1306_Handle *display, const SSD1306_Command *command);
SSD1306_Status SSD1306_Handler(SSD1306_Handle *display);
uint8_t SSD1306_HasCommands(SSD1306_Handle *display);

SSD1306_Status SSD1306_SetClearCommand(SSD1306_Handle *display);
SSD1306_Status SSD1306_SetFillCommand(SSD1306_Handle *display, uint8_t color);
SSD1306_Status SSD1306_SetPixelCommand(SSD1306_Handle *display, int16_t x, int16_t y, uint8_t color);
SSD1306_Status SSD1306_SetLineCommand(SSD1306_Handle *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
SSD1306_Status SSD1306_SetRectCommand(SSD1306_Handle *display, int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t color);
SSD1306_Status SSD1306_SetBitmapCommand(SSD1306_Handle *display, int16_t x, int16_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t color);
SSD1306_Status SSD1306_SetUpdateCommand(SSD1306_Handle *display);

SSD1306_Status SSD1306_ExecuteCommand(SSD1306_Handle *display, const SSD1306_Command *command);
SSD1306_Status SSD1306_ExecuteCommandList(SSD1306_Handle *display, const SSD1306_Command *commands, uint16_t count);

SSD1306_Status SSD1306_Init(SSD1306_Handle *display, I2C_LL_Handle *i2c, uint8_t address);
void SSD1306_Clear(SSD1306_Handle *display);
void SSD1306_Fill(SSD1306_Handle *display, uint8_t color);
SSD1306_Status SSD1306_Update(SSD1306_Handle *display);
void SSD1306_DrawPixel(SSD1306_Handle *display, uint8_t x, uint8_t y, uint8_t color);
void SSD1306_DrawLine(SSD1306_Handle *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void SSD1306_DrawRect(SSD1306_Handle *display, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);
void SSD1306_DrawBitmap(SSD1306_Handle *display, uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
