#ifndef OLED_APP_H
#define OLED_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ssd1306.h"

typedef enum
{
  OLED_APP_CMD_TEST_SHAPES = 0,
  OLED_APP_CMD_CLEAR,
  OLED_APP_CMD_FILL_WHITE
} OLED_AppCommand;

typedef struct
{
  I2C_TypeDef *I2Cx;
  uint32_t clock_speed;
  uint32_t timeout;
  uint8_t address;
  OLED_AppCommand command;
} OLED_AppConfig;

SSD1306_Status OLED_App_Run(const OLED_AppConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* OLED_APP_H */
