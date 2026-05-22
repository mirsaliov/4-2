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

/* 7-битный I2C адрес дисплея SSD1306. Чаще всего используется 0x3C */
#define SSD1306_I2C_ADDR_7BIT 0x3CU

/* Цвет пикселя: 0 — выключить пиксель, 1 — включить пиксель */
#define SSD1306_COLOR_BLACK 0U
#define SSD1306_COLOR_WHITE 1U

/* Статус выполнения функций драйвера SSD1306 */
typedef enum
{
  SSD1306_OK = 0,     /* Операция выполнена успешно */
  SSD1306_ERROR       /* Ошибка при выполнении операции */
} SSD1306_Status;

/*
 * Главная структура дисплея.
 * В ней хранится указатель на I2C, адрес дисплея, видеобуфер и флаг инициализации.
 */
typedef struct
{
  I2C_LL_Handle *i2c;                    /* Через какой I2C работает дисплей */
  uint8_t address;                       /* 7-битный I2C адрес дисплея */
  uint8_t buffer[SSD1306_BUFFER_SIZE];   /* Буфер изображения 128x64 */
  uint8_t initialized;                   /* 1 — дисплей инициализирован, 0 — нет */
} SSD1306_Handle;

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
