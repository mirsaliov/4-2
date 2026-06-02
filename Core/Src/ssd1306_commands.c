#include "ssd1306.h"

/*
 * Удобный старт дисплея.
 * Эта функция является верхним уровнем запуска.
 * Пользователь передает config, а здесь настройки переносятся в I2C-драйвер.
 */
SSD1306_Status SSD1306_Begin(SSD1306_Handle *display, I2C_LL_Handle *i2c, const SSD1306_Config *config)
{
  uint8_t address;

  /* Проверяем, что в функцию передали нормальные указатели */
  if ((display == 0) || (i2c == 0) || (config == 0))
  {
    return SSD1306_ERROR;
  }

  /* Берем адрес дисплея из пользовательской структуры */
  address = config->address;

  /* Если адрес не задан, используем стандартный адрес SSD1306: 0x3C */
  if (address == 0U)
  {
    address = SSD1306_I2C_ADDR_7BIT;
  }

  /*
   * Передаем настройки из SSD1306_Config в структуру I2C.
   * После этого I2C_LL_Init() уже знает:
   * какой I2C использовать, какая скорость и какой timeout.
   */
  i2c->I2Cx = config->I2Cx;
  i2c->clock_speed = config->clock_speed;
  i2c->timeout = config->timeout;

  /* Инициализируем выбранный I2C-модуль */
  if (I2C_LL_Init(i2c) != I2C_LL_OK)
  {
    return SSD1306_ERROR;
  }

  /* Инициализируем сам дисплей SSD1306 */
  if (SSD1306_Init(display, i2c, address) != SSD1306_OK)
  {
    return SSD1306_ERROR;
  }

  /* Очищаем очередь команд перед началом работы */
  SSD1306_ClearCommands(display);

  /* busy = 0 означает, что сейчас команда не выполняется */
  display->busy = 0U;

  /* WAIT означает, что драйвер готов ждать команды из очереди */
  display->state = SSD1306_STATE_WAIT;

  return SSD1306_OK;
}

/*
 * Очистка очереди команд.
 * Используется перед добавлением нового набора команд.
 */
void SSD1306_ClearCommands(SSD1306_Handle *display)
{
  if (display != 0)
  {
    /* head — индекс команды, которую надо выполнить следующей */
    display->head = 0U;

    /* tail — индекс места, куда будет записана новая команда */
    display->tail = 0U;

    /* count — сколько команд сейчас находится в очереди */
    display->count = 0U;

    /* busy — флаг занятости обработчика */
    display->busy = 0U;
  }
}

/*
 * Добавление команды в кольцевую очередь.
 * Эта функция принимает уже готовую структуру SSD1306_Command
 * и копирует ее внутрь display->commands[].
 */
SSD1306_Status SSD1306_AddCommand(SSD1306_Handle *display, const SSD1306_Command *command)
{
  if ((display == 0) || (command == 0))
  {
    return SSD1306_ERROR;
  }

  /* Если count равен размеру очереди, значит свободного места больше нет */
  if (display->count >= SSD1306_COMMAND_QUEUE_SIZE)
  {
    return SSD1306_ERROR;
  }

  /* Копируем команду в конец очереди */
  display->commands[display->tail] = *command;

  /*
   * Сдвигаем tail на следующую позицию.
   * Оператор % делает очередь кольцевой:
   * если tail дошел до конца массива, он снова станет 0.
   */
  display->tail = (uint8_t)((display->tail + 1U) % SSD1306_COMMAND_QUEUE_SIZE);

  /* Увеличиваем количество команд в очереди */
  display->count++;

  return SSD1306_OK;
}

/*
 * Добавить команду очистки буфера.
 * Эта команда не очищает OLED сразу, а только добавляется в очередь.
 */
SSD1306_Status SSD1306_SetClearCommand(SSD1306_Handle *display)
{
  SSD1306_Command command = {0};

  /* Указываем тип команды: очистка буфера */
  command.type = SSD1306_CMD_CLEAR;

  /* Добавляем созданную команду в очередь */
  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду заливки всего буфера цветом.
 */
SSD1306_Status SSD1306_SetFillCommand(SSD1306_Handle *display, uint8_t color)
{
  SSD1306_Command command = {0};

  /* Тип команды — заливка */
  command.type = SSD1306_CMD_FILL;

  /* Цвет заливки: SSD1306_COLOR_BLACK или SSD1306_COLOR_WHITE */
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду рисования одного пикселя.
 */
SSD1306_Status SSD1306_SetPixelCommand(SSD1306_Handle *display, int16_t x, int16_t y, uint8_t color)
{
  SSD1306_Command command = {0};

  /* Тип команды — пиксель */
  command.type = SSD1306_CMD_PIXEL;

  /* Координаты пикселя */
  command.x0 = x;
  command.y0 = y;

  /* Цвет пикселя */
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду рисования линии.
 * Значения x0, y0, x1, y1 сначала сохраняются в структуру команды,
 * а потом через Handler попадут в SSD1306_DrawLine().
 */
SSD1306_Status SSD1306_SetLineCommand(SSD1306_Handle *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
{
  SSD1306_Command command = {0};

  /* Тип команды — линия */
  command.type = SSD1306_CMD_LINE;

  /* Начальная точка линии */
  command.x0 = x0;
  command.y0 = y0;

  /* Конечная точка линии */
  command.x1 = x1;
  command.y1 = y1;

  /* Цвет линии */
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду рисования прямоугольника.
 */
SSD1306_Status SSD1306_SetRectCommand(SSD1306_Handle *display, int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t color)
{
  SSD1306_Command command = {0};

  /* Тип команды — прямоугольник */
  command.type = SSD1306_CMD_RECT;

  /* Левый верхний угол прямоугольника */
  command.x0 = x;
  command.y0 = y;

  /* Размеры прямоугольника */
  command.width = width;
  command.height = height;

  /* Цвет прямоугольника */
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду вывода bitmap-картинки.
 */
SSD1306_Status SSD1306_SetBitmapCommand(SSD1306_Handle *display, int16_t x, int16_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, uint8_t color)
{
  SSD1306_Command command = {0};

  /* Тип команды — bitmap */
  command.type = SSD1306_CMD_BITMAP;

  /* Координаты левого верхнего угла картинки */
  command.x0 = x;
  command.y0 = y;

  /* Указатель на массив bitmap */
  command.bitmap = bitmap;

  /* Размеры bitmap */
  command.width = width;
  command.height = height;

  /* Цвет, которым рисуется bitmap */
  command.color = color;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Добавить команду обновления дисплея.
 * Без этой команды рисунок останется только в буфере STM32
 * и не появится на OLED.
 */
SSD1306_Status SSD1306_SetUpdateCommand(SSD1306_Handle *display)
{
  SSD1306_Command command = {0};

  /* Тип команды — отправить буфер на дисплей */
  command.type = SSD1306_CMD_UPDATE;

  return SSD1306_AddCommand(display, &command);
}

/*
 * Выполнение одной пользовательской команды.
 * Здесь switch смотрит на command->type и вызывает нужную функцию драйвера.
 */
SSD1306_Status SSD1306_ExecuteCommand(SSD1306_Handle *display, const SSD1306_Command *command)
{
  if ((display == 0) || (command == 0))
  {
    return SSD1306_ERROR;
  }

  /* command->type показывает, какую именно команду надо выполнить */
  switch (command->type)
  {
    case SSD1306_CMD_CLEAR:
      /* Очистить буфер в памяти STM32 */
      SSD1306_Clear(display);
      return SSD1306_OK;

    case SSD1306_CMD_FILL:
      /* Заполнить весь буфер выбранным цветом */
      SSD1306_Fill(display, command->color);
      return SSD1306_OK;

    case SSD1306_CMD_PIXEL:
      /* Нарисовать пиксель по координатам command->x0, command->y0 */
      SSD1306_DrawPixel(display,
                        (uint8_t)command->x0,
                        (uint8_t)command->y0,
                        command->color);
      return SSD1306_OK;

    case SSD1306_CMD_LINE:
      /*
       * Нарисовать линию.
       * Сюда приходят значения, которые были записаны в SSD1306_SetLineCommand().
       */
      SSD1306_DrawLine(display,
                       command->x0,
                       command->y0,
                       command->x1,
                       command->y1,
                       command->color);
      return SSD1306_OK;

    case SSD1306_CMD_RECT:
      /* Нарисовать прямоугольник по координатам и размерам из структуры команды */
      SSD1306_DrawRect(display,
                       (uint8_t)command->x0,
                       (uint8_t)command->y0,
                       command->width,
                       command->height,
                       command->color);
      return SSD1306_OK;

    case SSD1306_CMD_BITMAP:
      /* Нарисовать bitmap-картинку */
      SSD1306_DrawBitmap(display,
                         (uint8_t)command->x0,
                         (uint8_t)command->y0,
                         command->bitmap,
                         command->width,
                         command->height,
                         command->color);
      return SSD1306_OK;

    case SSD1306_CMD_UPDATE:
      /* Отправить буфер 128x64 из STM32 в OLED по I2C */
      return SSD1306_Update(display);

    default:
      /* Если type неизвестный, возвращаем ошибку */
      return SSD1306_ERROR;
  }
}

/*
 * Handler верхнего уровня.
 * Пока без прерываний и DMA: за один вызов выполняет один шаг состояния.
 * Поэтому его удобно вызывать в while(1).
 */
SSD1306_Status SSD1306_Handler(SSD1306_Handle *display)
{
  SSD1306_Status status;

  if (display == 0)
  {
    return SSD1306_ERROR;
  }

  /* state показывает, на каком этапе сейчас находится обработчик */
  switch (display->state)
  {
    case SSD1306_STATE_RESET:
      /* RESET: начальное состояние, переводим драйвер в ожидание */
      display->state = SSD1306_STATE_WAIT;
      return SSD1306_OK;

    case SSD1306_STATE_WAIT:
      /* WAIT: драйвер ждёт команду в очереди */
      if (display->count == 0U)
      {
        /* Команд нет, делать нечего */
        return SSD1306_OK;
      }

      /* Берем команду из начала очереди */
      display->current_command = display->commands[display->head];

      /* Помечаем, что драйвер занят */
      display->busy = 1U;

      /* Переходим к выполнению команды */
      display->state = SSD1306_STATE_WORK;
      return SSD1306_OK;

    case SSD1306_STATE_WORK:
      /* WORK: выполняем текущую команду */
      status = SSD1306_ExecuteCommand(display, &display->current_command);
      if (status != SSD1306_OK)
      {
        /* Если команда не выполнилась, переходим в состояние ошибки */
        display->busy = 0U;
        display->state = SSD1306_STATE_ERROR;
        return status;
      }

      /* Команда выполнена, можно переходить к удалению ее из очереди */
      display->state = SSD1306_STATE_READY;
      return SSD1306_OK;

    case SSD1306_STATE_READY:
      /* READY: команда выполнена, сдвигаем очередь */
      if (display->count > 0U)
      {
        /* Переходим к следующей команде в кольцевой очереди */
        display->head = (uint8_t)((display->head + 1U) % SSD1306_COMMAND_QUEUE_SIZE);

        /* Уменьшаем количество оставшихся команд */
        display->count--;
      }

      /* Команда завершена, драйвер больше не занят */
      display->busy = 0U;

      /* Возвращаемся в ожидание следующей команды */
      display->state = SSD1306_STATE_WAIT;
      return SSD1306_OK;

    case SSD1306_STATE_ERROR:
    default:
      /* ERROR или неизвестное состояние */
      return SSD1306_ERROR;
  }
}

/*
 * Проверка занятости драйвера.
 * Возвращает 1, если команда сейчас выполняется.
 */
uint8_t SSD1306_IsBusy(SSD1306_Handle *display)
{
  if (display == 0)
  {
    return 0U;
  }

  return display->busy;
}

/*
 * Проверка наличия команд в очереди.
 * Возвращает 1, если count больше 0.
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
 * Выполнение массива команд через очередь и handler.
 * Сейчас в main.c мы используем Set...Command(), но эта функция оставлена
 * как дополнительный вариант для выполнения готового массива SSD1306_Command[].
 */
SSD1306_Status SSD1306_ExecuteCommandList(SSD1306_Handle *display, const SSD1306_Command *commands, uint16_t count)
{
  uint16_t i;
  SSD1306_Status status;

  if ((display == 0) || (commands == 0))
  {
    return SSD1306_ERROR;
  }

  /* Перед загрузкой нового списка очищаем старую очередь */
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
