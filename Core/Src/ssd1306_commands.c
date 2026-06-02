#include "ssd1306.h"

/*
 * Верхний уровень запуска дисплея.
 * Пользователь передает структуру config, а функция сама:
 * 1) настраивает I2C-драйвер;
 * 2) инициализирует SSD1306;
 * 3) подготавливает очередь команд.
 */
SSD1306_Status SSD1306_Begin(SSD1306_Handle *display, I2C_LL_Handle *i2c, const SSD1306_Config *config)
{
  uint8_t address;

  /* Проверка указателей: если что-то не передали, работать нельзя */
  if ((display == 0) || (i2c == 0) || (config == 0))
  {
    return SSD1306_ERROR;
  }

  /* Берем адрес дисплея из пользовательской конфигурации */
  address = config->address;

  /* Если адрес равен 0, используем стандартный адрес SSD1306: 0x3C */
  if (address == 0U)
  {
    address = SSD1306_I2C_ADDR_7BIT;
  }

  /* Передаем настройки из SSD1306_Config в структуру I2C-драйвера */
  i2c->I2Cx = config->I2Cx;
  i2c->clock_speed = config->clock_speed;
  i2c->timeout = config->timeout;

  /* Настраиваем выбранный I2C: I2C1, I2C2 или I2C3 */
  if (I2C_LL_Init(i2c) != I2C_LL_OK)
  {
    return SSD1306_ERROR;
  }

  /* Отправляем команды инициализации в сам OLED-контроллер SSD1306 */
  if (SSD1306_Init(display, i2c, address) != SSD1306_OK)
  {
    return SSD1306_ERROR;
  }

  /* Очищаем очередь команд после запуска дисплея */
  SSD1306_ClearCommands(display);

  /* Состояние WAIT: драйвер готов ждать команды */
  display->state = SSD1306_STATE_WAIT;

  return SSD1306_OK;
}

/*
 * Очистка очереди команд.
 * head  — индекс команды, которая будет выполнена следующей.
 * count — количество команд в очереди.
 */
void SSD1306_ClearCommands(SSD1306_Handle *display)
{
  if (display != 0)
  {
    /* Начинаем выполнение с нулевого элемента массива commands[] */
    display->head = 0U;

    /* Команд в очереди больше нет */
    display->count = 0U;

    /* После очистки драйвер ждёт новые команды */
    display->state = SSD1306_STATE_WAIT;
  }
}

/*
 * Добавление команды в очередь.
 * Команда уже создана в виде структуры SSD1306_Command.
 * Здесь она копируется в массив display->commands[].
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
   * Индекс для новой команды считается от текущей head + count.
   * head показывает начало очереди, count показывает сколько команд уже есть.
   */
  index = (uint8_t)(display->head + display->count);

  /* Если индекс вышел за конец массива, переносим его в начало */
  if (index >= SSD1306_COMMAND_QUEUE_SIZE)
  {
    index = (uint8_t)(index - SSD1306_COMMAND_QUEUE_SIZE);
  }

  /* Копируем команду в рассчитанную позицию очереди */
  display->commands[index] = *command;

  /* Увеличиваем количество команд в очереди */
  display->count++;

  return SSD1306_OK;
}

/*
 * Добавить команду очистки буфера.
 * Важно: очищается буфер в памяти STM32, а не экран сразу.
 */
SSD1306_Status SSD1306_SetClearCommand(SSD1306_Handle *display)
{
  SSD1306_Command command = {0};

  /* Указываем тип команды */
  command.type = SSD1306_CMD_CLEAR;

  /* Добавляем команду в очередь */
  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду заливки всего буфера цветом.
 */
SSD1306_Status SSD1306_SetFillCommand(SSD1306_Handle *display, uint8_t color)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_FILL;
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду рисования одного пикселя.
 */
SSD1306_Status SSD1306_SetPixelCommand(SSD1306_Handle *display, int16_t x, int16_t y, uint8_t color)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_PIXEL;
  command.x0 = x;
  command.y0 = y;
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду рисования линии.
 * Значения x0, y0, x1, y1 сначала сохраняются в структуру,
 * а уже потом через Handler попадут в SSD1306_DrawLine().
 */
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

/*
 * Добавить команду рисования прямоугольника.
 * x, y — левый верхний угол.
 * width, height — ширина и высота.
 */
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

/*
 * Добавить команду рисования bitmap-картинки.
 */
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

/*
 * Добавить команду обновления дисплея.
 * Без этой команды изменения останутся только в buffer[]
 * и не появятся на OLED.
 */
SSD1306_Status SSD1306_SetUpdateCommand(SSD1306_Handle *display)
{
  SSD1306_Command command = {0};

  command.type = SSD1306_CMD_UPDATE;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Выполнение одной команды.
 * switch смотрит на command->type и выбирает нужную функцию.
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
      /* Очистить буфер изображения */
      SSD1306_Clear(display);
      return SSD1306_OK;

    case SSD1306_CMD_FILL:
      /* Залить весь буфер выбранным цветом */
      SSD1306_Fill(display, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_PIXEL:
      /* Нарисовать один пиксель */
      SSD1306_DrawPixel(display, (uint8_t)command->x0, (uint8_t)command->y0, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_LINE:
      /* Нарисовать линию по координатам из структуры команды */
      SSD1306_DrawLine(display, command->x0, command->y0, command->x1, command->y1, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_RECT:
      /* Нарисовать прямоугольник */
      SSD1306_DrawRect(display, (uint8_t)command->x0, (uint8_t)command->y0, command->width, command->height, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_BITMAP:
      /* Нарисовать bitmap */
      SSD1306_DrawBitmap(display, (uint8_t)command->x0, (uint8_t)command->y0, command->bitmap, command->width, command->height, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_UPDATE:
      /* Отправить buffer[] на OLED по I2C */
      return SSD1306_Update(display);

    default:
      return SSD1306_ERROR;
  }
}

/*
 * Обработчик верхнего уровня.
 * Его вызывают в while(1).
 * За один полный проход WAIT -> WORK выполняется одна команда из очереди.
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
    case SSD1306_STATE_WAIT:
      /* Если команд нет, ничего не делаем */
      if (display->count == 0U)
      {
        return SSD1306_OK;
      }

      /* Если команда есть, переходим к выполнению */
      display->state = SSD1306_STATE_WORK;
      return SSD1306_OK;

    case SSD1306_STATE_WORK:
      /* Выполняем команду, на которую сейчас указывает head */
      status = SSD1306_ExecuteCommand(display, &display->commands[display->head]);
      if (status != SSD1306_OK)
      {
        display->state = SSD1306_STATE_ERROR;
        return status;
      }

      /* Переходим к следующей команде */
      display->head++;
      if (display->head >= SSD1306_COMMAND_QUEUE_SIZE)
      {
        display->head = 0U;
      }

      /* Одну команду выполнили, значит команд стало меньше */
      display->count--;

      /* Возвращаемся в ожидание следующей команды */
      display->state = SSD1306_STATE_WAIT;
      return SSD1306_OK;

    case SSD1306_STATE_ERROR:
    default:
      return SSD1306_ERROR;
  }
}

/*
 * Проверка наличия команд.
 * Возвращает 1, если в очереди есть хотя бы одна команда.
 */
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
 * Сейчас main.c использует SSD1306_Set...Command(),
 * но эта функция оставлена как дополнительный вариант.
 */
SSD1306_Status SSD1306_ExecuteCommandList(SSD1306_Handle *display, const SSD1306_Command *commands, uint16_t count)
{
  uint16_t i;
  SSD1306_Status status;

  if ((display == 0) || (commands == 0))
  {
    return SSD1306_ERROR;
  }

  /* Сначала очищаем старую очередь */
  SSD1306_ClearCommands(display);

  /* Добавляем все команды из массива в очередь */
  for (i = 0U; i < count; i++)
  {
    status = SSD1306_AddCommand(display, &commands[i]);
    if (status != SSD1306_OK)
    {
      return status;
    }
  }

  /* Выполняем очередь до конца */
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
