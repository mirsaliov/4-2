#include "oled_app.h"

/*
 * Внутренние объекты приложения.
 * Пользователь не работает с ними напрямую.
 */
static I2C_LL_Handle app_i2c;
static SSD1306_Handle app_oled;

/*
 * Демонстрация графических возможностей драйвера.
 * Рисует пиксель, линию и прямоугольник.
 */
static SSD1306_Status OLED_App_TestShapes(void)
{
  SSD1306_Clear(&app_oled);

  SSD1306_DrawPixel(&app_oled, 10U, 10U, SSD1306_COLOR_WHITE);
  SSD1306_DrawLine(&app_oled, 0, 0, 127, 63, SSD1306_COLOR_WHITE);
  SSD1306_DrawRect(&app_oled, 20U, 16U, 50U, 30U, SSD1306_COLOR_WHITE);

  return SSD1306_Update(&app_oled);
}

/*
 * Очистка дисплея.
 */
static SSD1306_Status OLED_App_Clear(void)
{
  SSD1306_Clear(&app_oled);
  return SSD1306_Update(&app_oled);
}

/*
 * Заливка дисплея белым цветом.
 */
static SSD1306_Status OLED_App_FillWhite(void)
{
  SSD1306_Fill(&app_oled, SSD1306_COLOR_WHITE);
  return SSD1306_Update(&app_oled);
}

/*
 * Главная функция пользовательского уровня.
 * Пользователь передает структуру с выбранным I2C, скоростью, адресом и командой.
 * Вся логика инициализации и рисования спрятана внутри этого файла.
 */
SSD1306_Status OLED_App_Run(const OLED_AppConfig *config)
{
  uint8_t address;

  if (config == 0)
  {
    return SSD1306_ERROR;
  }

  address = config->address;
  if (address == 0U)
  {
    address = SSD1306_I2C_ADDR_7BIT;
  }

  app_i2c.I2Cx = config->I2Cx;
  app_i2c.clock_speed = config->clock_speed;
  app_i2c.timeout = config->timeout;

  if (I2C_LL_Init(&app_i2c) != I2C_LL_OK)
  {
    return SSD1306_ERROR;
  }

  if (SSD1306_Init(&app_oled, &app_i2c, address) != SSD1306_OK)
  {
    return SSD1306_ERROR;
  }

  switch (config->command)
  {
    case OLED_APP_CMD_TEST_SHAPES:
      return OLED_App_TestShapes();

    case OLED_APP_CMD_CLEAR:
      return OLED_App_Clear();

    case OLED_APP_CMD_FILL_WHITE:
      return OLED_App_FillWhite();

    default:
      return SSD1306_ERROR;
  }
}
