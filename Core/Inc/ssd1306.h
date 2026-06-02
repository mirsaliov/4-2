#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include "i2c_ll_driver.h"
#include <stdint.h>

/* Размер OLED-дисплея SSD1306: 128 пикселей по X и 64 пикселя по Y */
#define SSD1306_WIDTH 128U
#define SSD1306_HEIGHT 64U

/*
 * Размер буфера экрана.
 * На 1 пиксель нужен 1 бит.
 * 128 * 64 = 8192 бит = 1024 байта.
 */
#define SSD1306_BUFFER_SIZE ((SSD1306_WIDTH * SSD1306_HEIGHT) / 8U)

/* Размер очереди команд, как в примере с датчиком */
#define SSD1306_COMMAND_QUEUE_SIZE 16U

/* 7-битный I2C адрес дисплея SSD1306. Чаще всего используется 0x3C */
#define SSD1306_I2C_ADDR_7BIT 0x3CU

/* Цвет пикселя: 0 — выключить пиксель, 1 — включить пиксель */
#define SSD1306_COLOR_BLACK 0U
#define SSD1306_COLOR_WHITE 1U

/* Статус выполнения функций драйвера SSD1306 */
typedef enum
{
  SSD1306_OK = 0,
  SSD1306_ERROR
} SSD1306_Status;

/* Состояние верхнего уровня драйвера */
typedef enum
{
  SSD1306_STATE_RESET = 0,
  SSD1306_STATE_WAIT,
  SSD1306_STATE_WORK,
  SSD1306_STATE_READY,
  SSD1306_STATE_ERROR
} SSD1306_State;

/*
 * Пользовательская конфигурация дисплея.
 * В main.c пользователь выбирает только I2C, скорость, timeout и адрес дисплея.
 */
typedef struct
{
  I2C_TypeDef *I2Cx;      /* Какой I2C использовать: I2C1, I2C2 или I2C3 */
  uint32_t clock_speed;   /* Скорость I2C в Гц, например 100000 = 100 kHz */
  uint32_t timeout;       /* Таймаут ожидания флагов */
  uint8_t address;        /* 7-битный адрес SSD1306, обычно 0x3C */
} SSD1306_Config;

/* Тип команды для пользователя */
typedef enum
{
  SSD1306_CMD_CLEAR = 0,  /* Очистить буфер */
  SSD1306_CMD_FILL,       /* Залить буфер цветом */
  SSD1306_CMD_PIXEL,      /* Нарисовать пиксель */
  SSD1306_CMD_LINE,       /* Нарисовать линию */
  SSD1306_CMD_RECT,       /* Нарисовать прямоугольник */
  SSD1306_CMD_BITMAP,     /* Нарисовать bitmap */
  SSD1306_CMD_UPDATE      /* Отправить буфер на дисплей */
} SSD1306_CommandType;

/*
 * Универсальная структура команды.
 * Пользователь выбирает type и заполняет только нужные поля.
 * Например для SSD1306_CMD_LINE используются x0, y0, x1, y1 и color.
 */
typedef struct
{
  SSD1306_CommandType type;

  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;

  uint8_t width;
  uint8_t height;
  uint8_t color;

  const uint8_t *bitmap;
} SSD1306_Command;

/*
 * Главная структура дисплея.
 * Здесь хранится состояние верхнего уровня и кольцевая очередь команд.
 */
typedef struct
{
  I2C_LL_Handle *i2c;                    /* Через какой I2C работает дисплей */
  uint8_t address;                       /* 7-битный I2C адрес дисплея */
  uint8_t buffer[SSD1306_BUFFER_SIZE];   /* Буфер изображения 128x64 */
  uint8_t initialized;                   /* 1 — дисплей инициализирован, 0 — нет */

  SSD1306_Command commands[SSD1306_COMMAND_QUEUE_SIZE];
  SSD1306_Command current_command;
  volatile uint8_t head;
  volatile uint8_t tail;
  volatile uint8_t count;
  volatile uint8_t busy;
  volatile SSD1306_State state;
} SSD1306_Handle;

/* Удобный пользовательский запуск: настраивает I2C и инициализирует SSD1306 */
SSD1306_Status SSD1306_Begin(SSD1306_Handle *display, I2C_LL_Handle *i2c, const SSD1306_Config *config);

/* Верхний уровень команд, как в примере с датчиком */
void SSD1306_ClearCommands(SSD1306_Handle *display);
SSD1306_Status SSD1306_AddCommand(SSD1306_Handle *display, const SSD1306_Command *command);
SSD1306_Status SSD1306_Handler(SSD1306_Handle *display);
uint8_t SSD1306_IsBusy(SSD1306_Handle *display);
uint8_t SSD1306_HasCommands(SSD1306_Handle *display);

/* Выполнение одной команды или массива команд */
SSD1306_Status SSD1306_ExecuteCommand(SSD1306_Handle *display, const SSD1306_Command *command);
SSD1306_Status SSD1306_ExecuteCommandList(SSD1306_Handle *display, const SSD1306_Command *commands, uint16_t count);

/* Инициализация дисплея SSD1306 */
SSD1306_Status SSD1306_Init(SSD1306_Handle *display, I2C_LL_Handle *i2c, uint8_t address);

/* Очистка буфера дисплея черным цветом */
void SSD1306_Clear(SSD1306_Handle *display);

/* Заполнение всего буфера выбранным цветом */
void SSD1306_Fill(SSD1306_Handle *display, uint8_t color);

/* Отправка буфера из памяти STM32 в дисплей */
SSD1306_Status SSD1306_Update(SSD1306_Handle *display);

/* Рисование одного пикселя в буфере */
void SSD1306_DrawPixel(SSD1306_Handle *display, uint8_t x, uint8_t y, uint8_t color);

/* Рисование линии в буфере */
void SSD1306_DrawLine(SSD1306_Handle *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);

/* Рисование прямоугольника в буфере */
void SSD1306_DrawRect(SSD1306_Handle *display, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color);

/* Рисование готовой bitmap-картинки в буфере */
void SSD1306_DrawBitmap(SSD1306_Handle *display, uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
