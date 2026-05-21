#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include "i2c_ll_driver.h"
#include <stdint.h>

#define SSD1306_WIDTH 128U
#define SSD1306_HEIGHT 64U
#define SSD1306_BUFFER_SIZE ((SSD1306_WIDTH * SSD1306_HEIGHT) / 8U)

#define SSD1306_I2C_ADDR_7BIT 0x3CU
#define SSD1306_COLOR_BLACK 0U
#define SSD1306_COLOR_WHITE 1U

typedef enum
{
  SSD1306_OK = 0,
  SSD1306_ERROR
} SSD1306_Status;

typedef struct
{
  I2C_LL_Handle *i2c;
  uint8_t address;
  uint8_t buffer[SSD1306_BUFFER_SIZE];
  uint8_t initialized;
} SSD1306_Handle;

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
