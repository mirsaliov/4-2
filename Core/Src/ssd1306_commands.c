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

  if (SSD1306_Init(display, i2c, address) != SSD1306_OK)
  {
    return SSD1306_ERROR;
  }

  SSD1306_ClearCommands(display);
  display->busy = 0U;
  display->state = SSD1306_STATE_WAIT;

  return SSD1306_OK;
}

/*
 * Очистка очереди команд.
 */
void SSD1306_ClearCommands(SSD1306_Handle *display)
{
  if (display != 0)
  {
    display->head = 0U;
    display->tail = 0U;
    display->count = 0U;
    display->busy = 0U;
  }
}

/*
 * Добавление команды в кольцевую очередь.
 */
SSD1306_Status SSD1306_AddCommand(SSD1306_Handle *display, const SSD1306_Command *command)
{
  if ((display == 0) || (command == 0))
  {
    return SSD1306_ERROR;
  }

  if (display->count >= SSD1306_COMMAND_QUEUE_SIZE)
  {
    return SSD1306_ERROR;
  }

  display->commands[display->tail] = *command;
  display->tail = (uint8_t)((display->tail + 1U) % SSD1306_COMMAND_QUEUE_SIZE);
  display->count++;

  return SSD1306_OK;
}

/*
 * Выполнение одной пользовательской команды.
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
 * Handler верхнего уровня.
 * Пока без прерываний и DMA: за один вызов выполняет одну команду из очереди.
 */
SSD1306_Status SSD1306_Handler(SSD1306_Handle *display)
{
  SSD1306_Status status;

  if (display == 0)
  {
    return SSD1306_ERROR;
  }

  switch (display->state)
  {
    case SSD1306_STATE_RESET:
      display->state = SSD1306_STATE_WAIT;
      return SSD1306_OK;

    case SSD1306_STATE_WAIT:
      if (display->count == 0U)
      {
        return SSD1306_OK;
      }

      display->current_command = display->commands[display->head];
      display->busy = 1U;
      display->state = SSD1306_STATE_WORK;
      return SSD1306_OK;

    case SSD1306_STATE_WORK:
      status = SSD1306_ExecuteCommand(display, &display->current_command);
      if (status != SSD1306_OK)
      {
        display->busy = 0U;
        display->state = SSD1306_STATE_ERROR;
        return status;
      }

      display->state = SSD1306_STATE_READY;
      return SSD1306_OK;

    case SSD1306_STATE_READY:
      if (display->count > 0U)
      {
        display->head = (uint8_t)((display->head + 1U) % SSD1306_COMMAND_QUEUE_SIZE);
        display->count--;
      }

      display->busy = 0U;
      display->state = SSD1306_STATE_WAIT;
      return SSD1306_OK;

    case SSD1306_STATE_ERROR:
    default:
      return SSD1306_ERROR;
  }
}

uint8_t SSD1306_IsBusy(SSD1306_Handle *display)
{
  if (display == 0)
  {
    return 0U;
  }

  return display->busy;
}

uint8_t SSD1306_HasCommands(SSD1306_Handle *display)
{
  if (display == 0)
  {
    return 0U;
  }

  return (display->count > 0U) ? 1U : 0U;
}

/*
 * Выполнение массива команд через очередь и handler.
 * Это вспомогательная функция, чтобы можно было быстро выполнить список команд без while в main.c.
 */
SSD1306_Status SSD1306_ExecuteCommandList(SSD1306_Handle *display, const SSD1306_Command *commands, uint16_t count)
{
  uint16_t i;
  SSD1306_Status status;

  if ((display == 0) || (commands == 0))
  {
    return SSD1306_ERROR;
  }

  SSD1306_ClearCommands(display);

  for (i = 0U; i < count; i++)
  {
    status = SSD1306_AddCommand(display, &commands[i]);
    if (status != SSD1306_OK)
    {
      return status;
    }
  }

  while ((SSD1306_HasCommands(display) != 0U) || (SSD1306_IsBusy(display) != 0U))
  {
    status = SSD1306_Handler(display);
    if (status != SSD1306_OK)
    {
      return status;
    }
  }

  return SSD1306_OK;
}
