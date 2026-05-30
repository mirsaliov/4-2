#include "ssd1306.h"
#include <string.h>

/*
 * Control byte для SSD1306.
 * 0x00 означает, что следующий байт является командой.
 */
#define SSD1306_CONTROL_COMMAND 0x00U

/*
 * Control byte для SSD1306.
 * 0x40 означает, что следующие байты являются данными для GDDRAM.
 */
#define SSD1306_CONTROL_DATA 0x40U

/*
 * SSD1306 128x64 делится на 8 страниц.
 * Одна страница = 8 пикселей по высоте.
 * 64 / 8 = 8 страниц.
 */
#define SSD1306_PAGE_COUNT 8U

/*
 * Отправка одной команды в SSD1306.
 * Сначала отправляется control byte 0x00, потом сама команда.
 */
static SSD1306_Status SSD1306_WriteCommand(SSD1306_Handle *display, uint8_t command)
{
  uint8_t data[2];

  data[0] = SSD1306_CONTROL_COMMAND;
  data[1] = command;

  if (I2C_LL_Write(display->i2c, display->address, data, 2U) != I2C_LL_OK)
  {
    return SSD1306_ERROR;
  }

  return SSD1306_OK;
}

/*
 * Отправка списка команд в SSD1306.
 * Используется при инициализации дисплея.
 */
static SSD1306_Status SSD1306_WriteCommandList(SSD1306_Handle *display, const uint8_t *commands, uint16_t size)
{
  uint16_t i;

  for (i = 0U; i < size; i++)
  {
    if (SSD1306_WriteCommand(display, commands[i]) != SSD1306_OK)
    {
      return SSD1306_ERROR;
    }
  }

  return SSD1306_OK;
}

/*
 * Простая функция модуля числа.
 * Нужна для алгоритма рисования линии.
 */
static int16_t SSD1306_Abs(int16_t value)
{
  return (value < 0) ? (int16_t)(-value) : value;
}

/*
 * Инициализация дисплея SSD1306.
 * Здесь задаются основные режимы работы контроллера дисплея.
 */
SSD1306_Status SSD1306_Init(SSD1306_Handle *display, I2C_LL_Handle *i2c, uint8_t address)
{
  static const uint8_t init_commands[] =
  {
    0xAE,       /* Display OFF */
    0x20, 0x00, /* Memory Addressing Mode: horizontal */
    0xB0,       /* Page Start Address */
    0xC8,       /* COM Output Scan Direction */
    0x00,       /* Lower Column Start Address */
    0x10,       /* Higher Column Start Address */
    0x40,       /* Display Start Line */
    0x81, 0x7F, /* Contrast Control */
    0xA1,       /* Segment Re-map */
    0xA6,       /* Normal Display */
    0xA8, 0x3F, /* Multiplex Ratio: 64 */
    0xA4,       /* Display RAM content */
    0xD3, 0x00, /* Display Offset */
    0xD5, 0x80, /* Display Clock */
    0xD9, 0xF1, /* Pre-charge Period */
    0xDA, 0x12, /* COM Pins Hardware Configuration */
    0xDB, 0x40, /* VCOMH Deselect Level */
    0x8D, 0x14, /* Charge Pump ON */
    0xAF        /* Display ON */
  };

  if ((display == 0) || (i2c == 0))
  {
    return SSD1306_ERROR;
  }

  memset(display->buffer, 0x00, SSD1306_BUFFER_SIZE);

  display->i2c = i2c;
  display->address = address;
  display->initialized = 0U;

  if (display->address == 0U)
  {
    display->address = SSD1306_I2C_ADDR_7BIT;
  }

  if (SSD1306_WriteCommandList(display, init_commands, (uint16_t)sizeof(init_commands)) != SSD1306_OK)
  {
    return SSD1306_ERROR;
  }

  display->initialized = 1U;

  SSD1306_Clear(display);
  return SSD1306_Update(display);
}

/*
 * Очистка буфера дисплея.
 * Важно: эта функция очищает только буфер в памяти STM32.
 * Чтобы изменения появились на экране, нужно вызвать SSD1306_Update().
 */
void SSD1306_Clear(SSD1306_Handle *display)
{
  if (display != 0)
  {
    memset(display->buffer, 0x00, SSD1306_BUFFER_SIZE);
  }
}

/*
 * Заполнение всего буфера одним цветом.
 * BLACK = 0x00, WHITE = 0xFF.
 */
void SSD1306_Fill(SSD1306_Handle *display, uint8_t color)
{
  if (display == 0)
  {
    return;
  }

  memset(display->buffer, (color == SSD1306_COLOR_BLACK) ? 0x00 : 0xFF, SSD1306_BUFFER_SIZE);
}

/*
 * Обновление дисплея.
 * Функция отправляет весь буфер 1024 байта из STM32 в GDDRAM дисплея.
 */
SSD1306_Status SSD1306_Update(SSD1306_Handle *display)
{
  uint8_t page;
  uint8_t data[SSD1306_WIDTH + 1U];

  if ((display == 0) || (display->i2c == 0))
  {
    return SSD1306_ERROR;
  }

  for (page = 0U; page < SSD1306_PAGE_COUNT; page++)
  {
    if (SSD1306_WriteCommand(display, (uint8_t)(0xB0U + page)) != SSD1306_OK)
    {
      return SSD1306_ERROR;
    }

    if (SSD1306_WriteCommand(display, 0x00U) != SSD1306_OK)
    {
      return SSD1306_ERROR;
    }

    if (SSD1306_WriteCommand(display, 0x10U) != SSD1306_OK)
    {
      return SSD1306_ERROR;
    }

    data[0] = SSD1306_CONTROL_DATA;
    memcpy(&data[1], &display->buffer[SSD1306_WIDTH * page], SSD1306_WIDTH);

    if (I2C_LL_Write(display->i2c, display->address, data, (uint16_t)(SSD1306_WIDTH + 1U)) != I2C_LL_OK)
    {
      return SSD1306_ERROR;
    }
  }

  return SSD1306_OK;
}

/*
 * Рисование одного пикселя в буфере.
 * Координаты: x = 0..127, y = 0..63.
 */
void SSD1306_DrawPixel(SSD1306_Handle *display, uint8_t x, uint8_t y, uint8_t color)
{
  uint16_t index;
  uint8_t mask;

  if ((display == 0) || (x >= SSD1306_WIDTH) || (y >= SSD1306_HEIGHT))
  {
    return;
  }

  index = (uint16_t)x + (uint16_t)(y / 8U) * SSD1306_WIDTH;
  mask = (uint8_t)(1U << (y % 8U));

  if (color == SSD1306_COLOR_BLACK)
  {
    display->buffer[index] &= (uint8_t)(~mask);
  }
  else
  {
    display->buffer[index] |= mask;
  }
}

/*
 * Рисование линии алгоритмом Брезенхэма.
 */
void SSD1306_DrawLine(SSD1306_Handle *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
  int16_t dx;
  int16_t dy;
  int16_t sx;
  int16_t sy;
  int16_t err;
  int16_t e2;

  if (display == 0)
  {
    return;
  }

  dx = SSD1306_Abs((int16_t)(x1 - x0));
  dy = (int16_t)(-SSD1306_Abs((int16_t)(y1 - y0)));
  sx = (x0 < x1) ? 1 : -1;
  sy = (y0 < y1) ? 1 : -1;
  err = (int16_t)(dx + dy);

  while (1)
  {
    if ((x0 >= 0) && (y0 >= 0) && (x0 < (int16_t)SSD1306_WIDTH) && (y0 < (int16_t)SSD1306_HEIGHT))
    {
      SSD1306_DrawPixel(display, (uint8_t)x0, (uint8_t)y0, color);
    }

    if ((x0 == x1) && (y0 == y1))
    {
      break;
    }

    e2 = (int16_t)(2 * err);

    if (e2 >= dy)
    {
      err = (int16_t)(err + dy);
      x0 = (int16_t)(x0 + sx);
    }

    if (e2 <= dx)
    {
      err = (int16_t)(err + dx);
      y0 = (int16_t)(y0 + sy);
    }
  }
}

/*
 * Рисование прямоугольника.
 */
void SSD1306_DrawRect(SSD1306_Handle *display, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color)
{
  if ((display == 0) || (width == 0U) || (height == 0U))
  {
    return;
  }

  SSD1306_DrawLine(display, x, y, (int16_t)(x + width - 1U), y, color);
  SSD1306_DrawLine(display, x, y, x, (int16_t)(y + height - 1U), color);
  SSD1306_DrawLine(display, (int16_t)(x + width - 1U), y, (int16_t)(x + width - 1U), (int16_t)(y + height - 1U), color);
  SSD1306_DrawLine(display, x, (int16_t)(y + height - 1U), (int16_t)(x + width - 1U), (int16_t)(y + height - 1U), color);
}

/*
 * Рисование bitmap-картинки.
 */
void SSD1306_DrawBitmap(SSD1306_Handle *display, uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t color)
{
  uint8_t bx;
  uint8_t by;
  uint16_t bit_index;
  uint16_t byte_index;
  uint8_t bit_mask;

  if ((display == 0) || (bitmap == 0))
  {
    return;
  }

  for (by = 0U; by < height; by++)
  {
    for (bx = 0U; bx < width; bx++)
    {
      bit_index = (uint16_t)by * width + bx;
      byte_index = bit_index / 8U;
      bit_mask = (uint8_t)(0x80U >> (bit_index % 8U));

      if ((bitmap[byte_index] & bit_mask) != 0U)
      {
        SSD1306_DrawPixel(display, (uint8_t)(x + bx), (uint8_t)(y + by), color);
      }
    }
  }
}
