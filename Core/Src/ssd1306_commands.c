#include "ssd1306.h"

/*
 * Удобный старт дисплея.
 * Пользователь передает структуру SSD1306_Config,
 * а функция сама настраивает I2C и запускает SSD1306.
 */
SSD1306_Status SSD1306_Begin(SSD1306_Handle *display, I2C_LL_Handle *i2c, const SSD1306_Config *config)
{
  uint8_t address;

  if ((display == 0) || (i2c == 0) || (config == 0))
  {
    return SSD1306_ERROR;
  }

  address = config->address;
  if (address == 0U)
  {
    address = SSD1306_I2C_ADDR_7BIT;
  }

  i2c->I2Cx = config->I2Cx;
  i2c->clock_speed = config->clock_speed;
  i2c->timeout = config->timeout;

  if (I2C_LL_Init(i2c) != I2C_LL_OK)
  {
    return SSD1306_ERROR;
  }

  return SSD1306_Init(display, i2c, address);
}

/*
 * Выполнение одной пользовательской команды.
 * Команда передается как структура SSD1306_Command.
 */
SSD1306_Status SSD1306_ExecuteCommand(SSD1306_Handle *display, const SSD1306_Command *command)
{
  if ((display == 0) || (command == 0))
  {
    return SSD1306_ERROR;
  }

  switch (command->type)
  {
    case SSD1306_CMD_CLEAR:
      SSD1306_Clear(display);
      return SSD1306_OK;

    case SSD1306_CMD_FILL:
      SSD1306_Fill(display, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_PIXEL:
      SSD1306_DrawPixel(display,
                        (uint8_t)command->x0,
                        (uint8_t)command->y0,
                        command->color);
      return SSD1306_OK;

    case SSD1306_CMD_LINE:
      SSD1306_DrawLine(display,
                       command->x0,
                       command->y0,
                       command->x1,
                       command->y1,
                       command->color);
      return SSD1306_OK;

    case SSD1306_CMD_RECT:
      SSD1306_DrawRect(display,
                       (uint8_t)command->x0,
                       (uint8_t)command->y0,
                       command->width,
                       command->height,
                       command->color);
      return SSD1306_OK;

    case SSD1306_CMD_BITMAP:
      SSD1306_DrawBitmap(display,
                         (uint8_t)command->x0,
                         (uint8_t)command->y0,
                         command->bitmap,
                         command->width,
                         command->height,
                         command->color);
      return SSD1306_OK;

    case SSD1306_CMD_UPDATE:
      return SSD1306_Update(display);

    default:
      return SSD1306_ERROR;
  }
}

/*
 * Выполнение массива команд.
 * В main.c пользователь создает SSD1306_Command commands[] и передает сюда.
 */
SSD1306_Status SSD1306_ExecuteCommandList(SSD1306_Handle *display, const SSD1306_Command *commands, uint16_t count)
{
  uint16_t i;
  SSD1306_Status status;

  if ((display == 0) || (commands == 0))
  {
    return SSD1306_ERROR;
  }

  for (i = 0U; i < count; i++)
  {
    status = SSD1306_ExecuteCommand(display, &commands[i]);
    if (status != SSD1306_OK)
    {
      return status;
    }
  }

  return SSD1306_OK;
}
