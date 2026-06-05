#include "ssd1306.h"

/*
 * Файл ssd1306_commands.c
 * -----------------------
 * Это верхний уровень OLED-драйвера.
 *
 * Главная идея этого файла — очередь команд.
 * Пользователь в main.c не вызывает SSD1306_DrawLine() напрямую.
 * Вместо этого он добавляет команды:
 *
 *   SSD1306_SetClearCommand(&oled);
 *   SSD1306_SetLineCommand(&oled, ...);
 *   SSD1306_SetUpdateCommand(&oled);
 *
 * Эти команды попадают в массив:
 *
 *   display->commands[]
 *
 * А потом функция SSD1306_Handler() постепенно выполняет команды по одной.
 *
 * Зачем так сделано:
 * 1) main.c становится проще;
 * 2) можно вызывать Handler по таймеру;
 * 3) можно использовать polling и interrupt режимы;
 * 4) верхний уровень не зависит от конкретной реализации I2C-передачи.
 *
 * Общая цепочка:
 * main.c
 *   -> SSD1306_Set...Command()
 *   -> SSD1306_AddCommand()
 *   -> очередь commands[]
 *   -> SSD1306_Handler()
 *   -> SSD1306_ExecuteCommand()
 *   -> реальные функции из ssd1306.c
 */

/*
 * Верхний уровень запуска дисплея.
 * Пользователь передает структуру config, а функция сама:
 * 1) переносит настройки в I2C-драйвер;
 * 2) инициализирует I2C;
 * 3) инициализирует SSD1306;
 * 4) подготавливает очередь команд.
 */
SSD1306_Status SSD1306_Begin(SSD1306_Handle *display, I2C_LL_Handle *i2c, const SSD1306_Config *config)
{
  uint8_t address;

  if ((display == 0) || (i2c == 0) || (config == 0))
  {
    return SSD1306_ERROR;
  }

  /* Если пользователь не указал адрес, используем стандартный 0x3C */
  address = config->address;
  if (address == 0U)
  {
    address = SSD1306_I2C_ADDR_7BIT;
  }

  /* Переносим настройки из OLED-конфига в нижний I2C-драйвер */
  i2c->I2Cx = config->I2Cx;
  i2c->clock_speed = config->clock_speed;
  i2c->timeout = config->timeout;
  i2c->mode = config->mode;

  /* Настраиваем выбранный I2C */
  if (I2C_LL_Init(i2c) != I2C_LL_OK)
  {
    return SSD1306_ERROR;
  }

  /* Инициализацию OLED оставляем блокирующей, чтобы дисплей точно стартовал до основной работы */
  if (SSD1306_Init(display, i2c, address) != SSD1306_OK)
  {
    return SSD1306_ERROR;
  }

  /* После запуска очередь команд должна быть пустой */
  SSD1306_ClearCommands(display);
  display->state = SSD1306_STATE_WAIT;

  return SSD1306_OK;
}

/* Очистка очереди пользовательских команд */
void SSD1306_ClearCommands(SSD1306_Handle *display)
{
  if (display != 0)
  {
    /* head — индекс команды, которую будем выполнять следующей */
    display->head = 0U;

    /* count — сколько команд сейчас находится в очереди */
    display->count = 0U;

    /* WAIT — обработчик ждёт новую команду */
    display->state = SSD1306_STATE_WAIT;
  }
}

/*
 * Добавление команды в очередь.
 * command копируется в массив display->commands[].
 */
SSD1306_Status SSD1306_AddCommand(SSD1306_Handle *display, const SSD1306_Command *command)
{
  uint8_t index;

  if ((display == 0) || (command == 0))
  {
    return SSD1306_ERROR;
  }

  /* Если очередь заполнена, новую команду добавить нельзя */
  if (display->count >= SSD1306_COMMAND_QUEUE_SIZE)
  {
    return SSD1306_ERROR;
  }

  /*
   * В этой упрощённой очереди нет tail.
   * Новая команда кладётся в позицию: head + count.
   */
  index = (uint8_t)(display->head + display->count);

  /* Если дошли до конца массива, переносимся в начало */
  if (index >= SSD1306_COMMAND_QUEUE_SIZE)
  {
    index = (uint8_t)(index - SSD1306_COMMAND_QUEUE_SIZE);
  }

  /* Копируем команду в очередь */
  display->commands[index] = *command;

  /* Команд в очереди стало больше */
  display->count++;

  return SSD1306_OK;
}

/* Добавить в очередь команду очистки buffer[] */
SSD1306_Status SSD1306_SetClearCommand(SSD1306_Handle *display)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_CLEAR;

  return SSD1306_AddCommand(display, &command);
}

/* Добавить в очередь команду заливки всего buffer[] одним цветом */
SSD1306_Status SSD1306_SetFillCommand(SSD1306_Handle *display, uint8_t color)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_FILL;
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/* Добавить в очередь команду рисования одного пикселя */
SSD1306_Status SSD1306_SetPixelCommand(SSD1306_Handle *display, int16_t x, int16_t y, uint8_t color)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_PIXEL;
  command.x0 = x;
  command.y0 = y;
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/* Добавить в очередь команду рисования линии */
SSD1306_Status SSD1306_SetLineCommand(SSD1306_Handle *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_LINE;
  command.x0 = x0;
  command.y0 = y0;
  command.x1 = x1;
  command.y1 = y1;
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/* Добавить в очередь команду рисования прямоугольника */
SSD1306_Status SSD1306_SetRectCommand(SSD1306_Handle *display, int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t color)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_RECT;
  command.x0 = x;
  command.y0 = y;
  command.width = width;
  command.height = height;
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/* Добавить в очередь команду рисования bitmap-картинки */
SSD1306_Status SSD1306_SetBitmapCommand(SSD1306_Handle *display, int16_t x, int16_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t color)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_BITMAP;
  command.x0 = x;
  command.y0 = y;
  command.bitmap = bitmap;
  command.width = width;
  command.height = height;
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/* Добавить в очередь команду отправки buffer[] на OLED */
SSD1306_Status SSD1306_SetUpdateCommand(SSD1306_Handle *display)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_UPDATE;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Выполнение одной команды.
 * switch смотрит на command->type и вызывает нужную функцию нижнего уровня SSD1306.
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
      SSD1306_DrawPixel(display, (uint8_t)command->x0, (uint8_t)command->y0, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_LINE:
      SSD1306_DrawLine(display, command->x0, command->y0, command->x1, command->y1, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_RECT:
      SSD1306_DrawRect(display, (uint8_t)command->x0, (uint8_t)command->y0, command->width, command->height, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_BITMAP:
      SSD1306_DrawBitmap(display, (uint8_t)command->x0, (uint8_t)command->y0, command->bitmap, command->width, command->height, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_UPDATE:
      /* В polling режиме Update дождётся конца передачи, в interrupt режиме только запустит её */
      return SSD1306_Update(display);

    default:
      return SSD1306_ERROR;
  }
}

/*
 * Главный обработчик очереди команд.
 * Его вызывает main.c по флагу от TIM6.
 */
SSD1306_Status SSD1306_Handler(SSD1306_Handle *display)
{
  SSD1306_Status status;

  if ((display == 0) || (display->i2c == 0))
  {
    return SSD1306_ERROR;
  }

  /*
   * Неблокирующая логика для interrupt-режима.
   * Если I2C сейчас занят передачей, Handler ничего не ждёт,
   * а сразу выходит. Следующий вызов будет позже по TIM6.
   */
  if (I2C_LL_IsBusy(display->i2c) != 0U)
  {
    return SSD1306_OK;
  }

  /* Если нижний I2C-драйвер сообщил ошибку, переводим OLED в ERROR */
  if (I2C_LL_GetStatus(display->i2c) == I2C_LL_ERROR)
  {
    display->state = SSD1306_STATE_ERROR;
    return SSD1306_ERROR;
  }

  switch (display->state)
  {
    case SSD1306_STATE_WAIT:
      /* Если команд нет, делать нечего */
      if (display->count == 0U)
      {
        return SSD1306_OK;
      }

      /* Команда есть, переходим к её выполнению */
      display->state = SSD1306_STATE_WORK;
      return SSD1306_OK;

    case SSD1306_STATE_WORK:
      /* Выполняем команду, на которую указывает head */
      status = SSD1306_ExecuteCommand(display, &display->commands[display->head]);
      if (status != SSD1306_OK)
      {
        display->state = SSD1306_STATE_ERROR;
        return status;
      }

      /* Команда запущена/выполнена, переходим к следующей */
      display->head++;
      if (display->head >= SSD1306_COMMAND_QUEUE_SIZE)
      {
        display->head = 0U;
      }

      /* Одну команду обработали, значит команд стало меньше */
      display->count--;

      display->state = SSD1306_STATE_WAIT;
      return SSD1306_OK;

    case SSD1306_STATE_ERROR:
    default:
      return SSD1306_ERROR;
  }
}

/* Проверка, есть ли команды в очереди */
uint8_t SSD1306_HasCommands(SSD1306_Handle *display)
{
  if (display == 0)
  {
    return 0U;
  }

  return (display->count > 0U) ? 1U : 0U;
}

/*
 * Выполнение готового массива команд.
 * Сейчас в main.c это не обязательно использовать,
 * но функция оставлена как дополнительная возможность драйвера.
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

  while (SSD1306_HasCommands(display) != 0U)
  {
    status = SSD1306_Handler(display);
    if (status != SSD1306_OK)
    {
      return status;
    }
  }

  return SSD1306_OK;
}
