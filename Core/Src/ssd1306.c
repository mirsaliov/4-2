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

  data[0] = SSD1306_CONTROL_COMMAND; /* 0x00: дальше идет команда */
  data[1] = command;                 /* Команда SSD1306, например 0xAE или 0xAF */

  /* Отправляем два байта по I2C: control byte + command */
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

  /* Проходим по массиву команд и отправляем их по одной */
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
  /*
   * Список команд инициализации SSD1306.
   * Команды идут в том порядке, в котором их нужно отправить в дисплей.
   */
  static const uint8_t init_commands[] =
  {
    0xAE,       /* Display OFF: выключить дисплей перед настройкой */
    0x20, 0x00, /* Memory Addressing Mode: 0x00 = horizontal addressing mode */
    0xB0,       /* Page Start Address: стартовая страница 0 */
    0xC8,       /* COM Output Scan Direction: зеркалирование по вертикали */
    0x00,       /* Lower Column Start Address: младшие 4 бита адреса колонки */
    0x10,       /* Higher Column Start Address: старшие 4 бита адреса колонки */
    0x40,       /* Display Start Line: начало отображения с линии 0 */
    0x81, 0x7F, /* Contrast Control: яркость/контраст, значение 0x7F */
    0xA1,       /* Segment Re-map: зеркалирование по горизонтали */
    0xA6,       /* Normal Display: обычный режим, не инверсия */
    0xA8, 0x3F, /* Multiplex Ratio: 0x3F = 64 строки */
    0xA4,       /* Entire Display ON: отображать содержимое RAM, не включать все пиксели */
    0xD3, 0x00, /* Display Offset: смещение по вертикали = 0 */
    0xD5, 0x80, /* Display Clock Divide Ratio/Oscillator Frequency */
    0xD9, 0xF1, /* Pre-charge Period: настройка предзаряда */
    0xDA, 0x12, /* COM Pins Hardware Configuration для дисплея 128x64 */
    0xDB, 0x40, /* VCOMH Deselect Level: уровень отключения VCOMH */
    0x8D, 0x14, /* Charge Pump Setting: включить внутренний charge pump */
    0xAF        /* Display ON: включить дисплей после настройки */
  };

  /* Проверяем, что указатели не пустые */
  if ((display == 0) || (i2c == 0))
  {
    return SSD1306_ERROR;
  }

  /* Очищаем буфер изображения в структуре дисплея */
  memset(display->buffer, 0x00, SSD1306_BUFFER_SIZE);

  /* Запоминаем, через какой I2C будет работать дисплей */
  display->i2c = i2c;

  /* Запоминаем I2C адрес дисплея */
  display->address = address;

  /* Пока инициализация не завершена */
  display->initialized = 0U;

  /* Если адрес не передали, используем стандартный адрес SSD1306 0x3C */
  if (display->address == 0U)
  {
    display->address = SSD1306_I2C_ADDR_7BIT;
  }

  /* Отправляем все команды инициализации в дисплей */
  if (SSD1306_WriteCommandList(display, init_commands, (uint16_t)sizeof(init_commands)) != SSD1306_OK)
  {
    return SSD1306_ERROR;
  }

  /* Отмечаем, что дисплей успешно инициализирован */
  display->initialized = 1U;

  /* Очищаем буфер после инициализации */
  SSD1306_Clear(display);

  /* Отправляем пустой буфер на экран, чтобы дисплей стал чистым */
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
    memset(display->buffer, 0x00, SSD1306_BUFFER_SIZE); /* 0x00 = все пиксели выключены */
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

  /* Если color черный — все байты 0x00, если белый — все байты 0xFF */
  memset(display->buffer, (color == SSD1306_COLOR_BLACK) ? 0x00 : 0xFF, SSD1306_BUFFER_SIZE);
}

/*
 * Обновление дисплея.
 * Функция отправляет весь буфер 1024 байта из STM32 в GDDRAM дисплея.
 */
SSD1306_Status SSD1306_Update(SSD1306_Handle *display)
{
  uint8_t page;
  uint8_t data[SSD1306_WIDTH + 1U]; /* 128 байт строки страницы + 1 control byte */

  /* Проверяем, что дисплей и I2C существуют */
  if ((display == 0) || (display->i2c == 0))
  {
    return SSD1306_ERROR;
  }

  /* Отправляем данные постранично: всего 8 страниц */
  for (page = 0U; page < SSD1306_PAGE_COUNT; page++)
  {
    /* 0xB0 + page: выбрать страницу GDDRAM от 0 до 7 */
    if (SSD1306_WriteCommand(display, (uint8_t)(0xB0U + page)) != SSD1306_OK)
    {
      return SSD1306_ERROR;
    }

    /* 0x00: установить младшую часть адреса колонки в 0 */
    if (SSD1306_WriteCommand(display, 0x00U) != SSD1306_OK)
    {
      return SSD1306_ERROR;
    }

    /* 0x10: установить старшую часть адреса колонки в 0 */
    if (SSD1306_WriteCommand(display, 0x10U) != SSD1306_OK)
    {
      return SSD1306_ERROR;
    }

    /* Первый байт 0x40 говорит SSD1306, что дальше идут данные изображения */
    data[0] = SSD1306_CONTROL_DATA;

    /* Копируем 128 байт одной страницы из буфера дисплея */
    memcpy(&data[1], &display->buffer[SSD1306_WIDTH * page], SSD1306_WIDTH);

    /* Отправляем control byte + 128 байт данных выбранной страницы */
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

  /* Если указатель пустой или координаты вышли за экран — ничего не делаем */
  if ((display == 0) || (x >= SSD1306_WIDTH) || (y >= SSD1306_HEIGHT))
  {
    return;
  }

  /*
   * SSD1306 хранит 8 вертикальных пикселей в одном байте.
   * y / 8 выбирает страницу, x выбирает колонку.
   */
  index = (uint16_t)x + (uint16_t)(y / 8U) * SSD1306_WIDTH;

  /*
   * y % 8 выбирает конкретный бит внутри байта.
   * Например y=10: 10 % 8 = 2, значит нужен бит 2.
   */
  mask = (uint8_t)(1U << (y % 8U));

  if (color == SSD1306_COLOR_BLACK)
  {
    /* Черный цвет: сбрасываем нужный бит в 0 */
    display->buffer[index] &= (uint8_t)(~mask);
  }
  else
  {
    /* Белый цвет: устанавливаем нужный бит в 1 */
    display->buffer[index] |= mask;
  }
}

/*
 * Рисование линии алгоритмом Брезенхэма.
 * Линия строится из отдельных пикселей.
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

  /* dx — расстояние по X между началом и концом линии */
  dx = SSD1306_Abs((int16_t)(x1 - x0));

  /* dy — отрицательное расстояние по Y, так удобнее для алгоритма */
  dy = (int16_t)(-SSD1306_Abs((int16_t)(y1 - y0)));

  /* sx показывает, куда двигаться по X: вправо или влево */
  sx = (x0 < x1) ? 1 : -1;

  /* sy показывает, куда двигаться по Y: вниз или вверх */
  sy = (y0 < y1) ? 1 : -1;

  /* err — накопленная ошибка линии */
  err = (int16_t)(dx + dy);

  while (1)
  {
    /* Рисуем пиксель только если он попадает в область экрана */
    if ((x0 >= 0) && (y0 >= 0) && (x0 < (int16_t)SSD1306_WIDTH) && (y0 < (int16_t)SSD1306_HEIGHT))
    {
      SSD1306_DrawPixel(display, (uint8_t)x0, (uint8_t)y0, color);
    }

    /* Если дошли до конечной точки — линия готова */
    if ((x0 == x1) && (y0 == y1))
    {
      break;
    }

    /* e2 нужен для выбора следующего шага по X и/или Y */
    e2 = (int16_t)(2 * err);

    /* Если ошибка позволяет, двигаемся по X */
    if (e2 >= dy)
    {
      err = (int16_t)(err + dy);
      x0 = (int16_t)(x0 + sx);
    }

    /* Если ошибка позволяет, двигаемся по Y */
    if (e2 <= dx)
    {
      err = (int16_t)(err + dx);
      y0 = (int16_t)(y0 + sy);
    }
  }
}

/*
 * Рисование прямоугольника.
 * Прямоугольник состоит из 4 линий.
 */
void SSD1306_DrawRect(SSD1306_Handle *display, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t color)
{
  if ((display == 0) || (width == 0U) || (height == 0U))
  {
    return;
  }

  /* Верхняя сторона */
  SSD1306_DrawLine(display, x, y, (int16_t)(x + width - 1U), y, color);

  /* Левая сторона */
  SSD1306_DrawLine(display, x, y, x, (int16_t)(y + height - 1U), color);

  /* Правая сторона */
  SSD1306_DrawLine(display, (int16_t)(x + width - 1U), y, (int16_t)(x + width - 1U), (int16_t)(y + height - 1U), color);

  /* Нижняя сторона */
  SSD1306_DrawLine(display, x, (int16_t)(y + height - 1U), (int16_t)(x + width - 1U), (int16_t)(y + height - 1U), color);
}

/*
 * Рисование bitmap-картинки.
 * bitmap — массив байтов, где каждый бит соответствует одному пикселю картинки.
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

  /* Проходим по всем пикселям bitmap */
  for (by = 0U; by < height; by++)
  {
    for (bx = 0U; bx < width; bx++)
    {
      /* Номер пикселя в bitmap */
      bit_index = (uint16_t)by * width + bx;

      /* Номер байта, где лежит этот пиксель */
      byte_index = bit_index / 8U;

      /* Маска нужного бита внутри байта */
      bit_mask = (uint8_t)(0x80U >> (bit_index % 8U));

      /* Если бит в bitmap равен 1, рисуем пиксель */
      if ((bitmap[byte_index] & bit_mask) != 0U)
      {
        SSD1306_DrawPixel(display, (uint8_t)(x + bx), (uint8_t)(y + by), color);
      }
    }
  }
}
SSD1306_Status SSD1306_Begin(SSD1306_Handle *display,
                             I2C_LL_Handle *i2c,
                             const SSD1306_Config *config)
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

  return SSD1306_OK;
}

void SSD1306_ClearCommands(SSD1306_Handle *display)
{
  if (display != 0)
  {
    display->command_count = 0U;
  }
}

SSD1306_Status SSD1306_AddCommand(SSD1306_Handle *display,
                                  const SSD1306_Command *command)
{
  if ((display == 0) || (command == 0))
  {
    return SSD1306_ERROR;
  }

  if (display->command_count >= SSD1306_COMMAND_QUEUE_SIZE)
  {
    return SSD1306_ERROR;
  }

  display->commands[display->command_count] = *command;
  display->command_count++;

  return SSD1306_OK;
}

SSD1306_Status SSD1306_ExecuteCommands(SSD1306_Handle *display)
{
  uint8_t i;
  SSD1306_Command *cmd;
  SSD1306_Status status;

  if (display == 0)
  {
    return SSD1306_ERROR;
  }

  for (i = 0U; i < display->command_count; i++)
  {
    cmd = &display->commands[i];

    switch (cmd->type)
    {
      case SSD1306_CMD_CLEAR:
        SSD1306_Clear(display);
        break;

      case SSD1306_CMD_FILL:
        SSD1306_Fill(display, cmd->data.fill.color);
        break;

      case SSD1306_CMD_PIXEL:
        SSD1306_DrawPixel(display,
                          (uint8_t)cmd->data.pixel.x,
                          (uint8_t)cmd->data.pixel.y,
                          cmd->data.pixel.color);
        break;

      case SSD1306_CMD_LINE:
        SSD1306_DrawLine(display,
                         cmd->data.line.x0,
                         cmd->data.line.y0,
                         cmd->data.line.x1,
                         cmd->data.line.y1,
                         cmd->data.line.color);
        break;

      case SSD1306_CMD_RECT:
        SSD1306_DrawRect(display,
                         (uint8_t)cmd->data.rect.x,
                         (uint8_t)cmd->data.rect.y,
                         cmd->data.rect.width,
                         cmd->data.rect.height,
                         cmd->data.rect.color);
        break;

      case SSD1306_CMD_UPDATE:
        status = SSD1306_Update(display);
        if (status != SSD1306_OK)
        {
          return status;
        }
        break;

      default:
        return SSD1306_ERROR;
    }
  }

  SSD1306_ClearCommands(display);

  return SSD1306_OK;
}

SSD1306_Status SSD1306_SetClearCmd(SSD1306_Handle *display)
{
  SSD1306_Command item = {0};

  item.type = SSD1306_CMD_CLEAR;

  return SSD1306_AddCommand(display, &item);
}

SSD1306_Status SSD1306_SetFillCmd(SSD1306_Handle *display, uint8_t color)
{
  SSD1306_Command item = {0};

  item.type = SSD1306_CMD_FILL;
  item.data.fill.color = color;

  return SSD1306_AddCommand(display, &item);
}

SSD1306_Status SSD1306_SetPixelCmd(SSD1306_Handle *display,
                                   int16_t x,
                                   int16_t y,
                                   uint8_t color)
{
  SSD1306_Command item = {0};

  item.type = SSD1306_CMD_PIXEL;
  item.data.pixel.x = x;
  item.data.pixel.y = y;
  item.data.pixel.color = color;

  return SSD1306_AddCommand(display, &item);
}

SSD1306_Status SSD1306_SetLineCmd(SSD1306_Handle *display,
                                  int16_t x0,
                                  int16_t y0,
                                  int16_t x1,
                                  int16_t y1,
                                  uint8_t color)
{
  SSD1306_Command item = {0};

  item.type = SSD1306_CMD_LINE;
  item.data.line.x0 = x0;
  item.data.line.y0 = y0;
  item.data.line.x1 = x1;
  item.data.line.y1 = y1;
  item.data.line.color = color;

  return SSD1306_AddCommand(display, &item);
}

SSD1306_Status SSD1306_SetRectCmd(SSD1306_Handle *display,
                                  int16_t x,
                                  int16_t y,
                                  uint8_t width,
                                  uint8_t height,
                                  uint8_t color)
{
  SSD1306_Command item = {0};

  item.type = SSD1306_CMD_RECT;
  item.data.rect.x = x;
  item.data.rect.y = y;
  item.data.rect.width = width;
  item.data.rect.height = height;
  item.data.rect.color = color;

  return SSD1306_AddCommand(display, &item);
}

SSD1306_Status SSD1306_SetUpdateCmd(SSD1306_Handle *display)
{
  SSD1306_Command item = {0};

  item.type = SSD1306_CMD_UPDATE;

  return SSD1306_AddCommand(display, &item);
}
