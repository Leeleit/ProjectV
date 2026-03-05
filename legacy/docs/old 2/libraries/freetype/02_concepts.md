# Концепции FreeType

🟡 **Уровень 2: Средний**

Ключевые концепции библиотеки: объекты, фиксированная точка, метрики глифов, таблицы символов, hinting и кернинг.

## Иерархия объектов

### FT_Library

Корневой объект библиотеки. Управляет модулями, драйверами и общими ресурсами.

```cpp
FT_Library library;
FT_Init_FreeType(&library);   // Создание
FT_Done_FreeType(library);    // Уничтожение
```

Свойства:

- Владеет всеми загруженными face объектами
- Управляет модулями (font drivers, renderers)
- Содержит memory manager и rasterizer

Многопоточность: Один `FT_Library` на поток безопасен. Для совместного использования между потоками требуется mutex
вокруг `FT_New_Face`/`FT_Done_Face`.

### FT_Face

Объект шрифта. Содержит все данные о загруженном шрифте.

```cpp
FT_Face face;
FT_New_Face(library, "font.ttf", 0, &face);
FT_Done_Face(face);
```

Ключевые поля:

| Поле           | Тип          | Описание                            |
|----------------|--------------|-------------------------------------|
| `num_faces`    | FT_Long      | Количество face в файле             |
| `face_index`   | FT_Long      | Индекс текущего face                |
| `num_glyphs`   | FT_Long      | Количество глифов                   |
| `family_name`  | FT_String*   | Имя семейства                       |
| `style_name`   | FT_String*   | Имя стиля                           |
| `units_per_EM` | FT_UShort    | Единиц на EM (обычно 2048 или 1000) |
| `bbox`         | FT_BBox      | Bounding box шрифта                 |
| `ascender`     | FT_Short     | Верхняя линия                       |
| `descender`    | FT_Short     | Нижняя линия (отрицательное)        |
| `height`       | FT_Short     | Высота строки                       |
| `glyph`        | FT_GlyphSlot | Текущий глиф                        |
| `size`         | FT_Size      | Текущий размер                      |
| `charmap`      | FT_CharMap   | Текущая таблица символов            |

Макросы проверки свойств:

```cpp
FT_IS_SCALABLE(face)      // Векторный шрифт
FT_IS_SFNT(face)          // SFNT-based (TrueType, OpenType)
FT_IS_FIXED_WIDTH(face)   // Моноширинный
FT_HAS_KERNING(face)      // Есть кернинг
FT_HAS_GLYPH_NAMES(face)  // Есть имена глифов
FT_HAS_COLOR(face)        // Есть цветные глифы
FT_IS_TRICKY(face)        // Требует нативного hinting
```

### FT_Size

Размер шрифта. Определяет масштабирование и hinting.

```cpp
FT_Size size;
FT_New_Size(face, &size);      // Создание дополнительного size
FT_Activate_Size(size);         // Активация
FT_Done_Size(size);             // Уничтожение
```

Метрики размера (в `size->metrics`):

| Поле        | Описание                            |
|-------------|-------------------------------------|
| `x_ppem`    | Пикселей на EM по горизонтали       |
| `y_ppem`    | Пикселей на EM по вертикали         |
| `x_scale`   | 16.16 коэффициент масштабирования X |
| `y_scale`   | 16.16 коэффициент масштабирования Y |
| `ascender`  | Ascender в 26.6 пикселях            |
| `descender` | Descender в 26.6 пикселях           |
| `height`    | Высота строки в 26.6 пикселях       |

### FT_GlyphSlot

Контейнер для загруженного глифа. Один на face, переиспользуется при каждой загрузке.

```cpp
FT_GlyphSlot slot = face->glyph;
```

Ключевые поля:

| Поле                | Тип              | Описание                         |
|---------------------|------------------|----------------------------------|
| `metrics`           | FT_Glyph_Metrics | Метрики глифа                    |
| `linearHoriAdvance` | FT_Fixed         | Advance без hinting (16.16)      |
| `linearVertAdvance` | FT_Fixed         | Вертикальный advance (16.16)     |
| `advance`           | FT_Vector        | Advance с hinting (26.6)         |
| `format`            | FT_Glyph_Format  | Формат (outline, bitmap, etc.)   |
| `bitmap`            | FT_Bitmap        | Растеризованный битмап           |
| `bitmap_left`       | FT_Int           | Смещение X битмапа               |
| `bitmap_top`        | FT_Int           | Смещение Y битмапа (от baseline) |
| `outline`           | FT_Outline       | Векторный контур                 |

## Фиксированная точка

FreeType не использует floating-point. Вместо этого применяются форматы с фиксированной точкой.

### Формат 26.6

26 бит целая часть, 6 бит дробная. Единица = 1/64 пикселя.

```cpp
// Конвертация из int в 26.6
FT_Pos fixed = int_value << 6;

// Конвертация из 26.6 в int (округление вниз)
int integer = fixed >> 6;

// Конвертация в float
float pixels = fixed / 64.0f;

// Пример: 16pt при 96dpi = 21.333... пикселей
// В 26.6: 21.333 * 64 = 1365 (0x555)
```

### Формат 16.16

16 бит целая часть, 16 бит дробная. Используется для коэффициентов масштабирования.

```cpp
FT_Fixed scale = face->size->metrics.x_scale;

// Применение масштабирования
FT_Pos scaled = FT_MulFix(font_units, scale);

// Эквивалентно:
FT_Pos scaled = (font_units * scale) >> 16;
```

### FT_Pos и FT_Fixed

```cpp
typedef signed long  FT_Pos;    // 32-битное целое
typedef signed int   FT_Fixed;  // 16.16 fixed-point
typedef signed long  FT_F26Dot6; // 26.6 fixed-point
```

## Метрики глифов

```
                    +-------------------+
                    |        ^          |
                    |        |          |
                    |   bearingY        |
                    |        v          |
            bearingX>------+---+   +----+
                    |      |   |   |    |
                    |      |   v   |    |
        baseline ---+------+-------+----+---
                    |      |   ^   |    |
                    |      |   |   |    |
                    |      | height  |    |
                    |      |   |   |    |
                    |      +---+   +----+
                    |                  |
                    |<---- width ----->|
                    |                  |
                    |<------ advance -------->|
```

Структура `FT_Glyph_Metrics`:

| Поле           | Описание                      | Единицы             |
|----------------|-------------------------------|---------------------|
| `width`        | Ширина bounding box           | 26.6 или font units |
| `height`       | Высота bounding box           | 26.6 или font units |
| `horiBearingX` | Left bearing (горизонтальный) | 26.6 или font units |
| `horiBearingY` | Top bearing (горизонтальный)  | 26.6 или font units |
| `horiAdvance`  | Advance width                 | 26.6 или font units |
| `vertBearingX` | Left bearing (вертикальный)   | 26.6 или font units |
| `vertBearingY` | Top bearing (вертикальный)    | 26.6 или font units |
| `vertAdvance`  | Advance height                | 26.6 или font units |

## Таблицы символов (CharMap)

CharMap преобразует коды символов в индексы глифов.

### Структура

```cpp
typedef struct FT_CharMapRec_ {
    FT_Face      face;
    FT_Encoding  encoding;
    FT_UShort    platform_id;
    FT_UShort    encoding_id;
} FT_CharMapRec;
```

### Типы кодировок

```cpp
FT_ENCODING_UNICODE        // Unicode (UTF-16 BMP)
FT_ENCODING_MS_SYMBOL      // Microsoft Symbol
FT_ENCODING_SJIS           // Shift JIS
FT_ENCODING_PRC            // Simplified Chinese
FT_ENCODING_BIG5           // Traditional Chinese
FT_ENCODING_WANSUNG        // Korean Wansung
FT_ENCODING_JOHAB          // Korean Johab
FT_ENCODING_ADOBE_LATIN_1  // Adobe Latin-1
FT_ENCODING_ADOBE_STANDARD // Adobe Standard
FT_ENCODING_APPLE_ROMAN    // Apple Roman
```

### Выбор CharMap

```cpp
// По типу кодировки
FT_Select_Charmap(face, FT_ENCODING_UNICODE);

// По указателю
FT_Set_Charmap(face, face->charmaps[0]);

// Получение индекса charmap
int index = FT_Get_Charmap_Index(face->charmap);
```

### Итерация по символам

```cpp
FT_ULong charcode;
FT_UInt  glyph_index;

charcode = FT_Get_First_Char(face, &glyph_index);
while (glyph_index != 0) {
    // Обработка (charcode, glyph_index)

    charcode = FT_Get_Next_Char(face, charcode, &glyph_index);
}
```

## Hinting

Hinting подгоняет контуры глифов к пиксельной сетке для лучшей читаемости.

### Режимы hinting

```cpp
FT_LOAD_TARGET_NORMAL  // Стандартный hinting
FT_LOAD_TARGET_LIGHT   // Лёгкий hinting (только по вертикали)
FT_LOAD_TARGET_MONO    // Монохромный hinting (чёрно-белый)
FT_LOAD_TARGET_LCD     // LCD-оптимизированный
FT_LOAD_TARGET_LCD_V   // Вертикальный LCD
```

### Флаги управления

```cpp
FT_LOAD_NO_HINTING       // Отключить hinting
FT_LOAD_FORCE_AUTOHINT   // Использовать auto-hinter
FT_LOAD_NO_AUTOHINT      // Отключить auto-hinter
```

### Tricky fonts

Некоторые шрифты требуют нативного hinting:

```cpp
if (FT_IS_TRICKY(face)) {
    // Обязательно использовать нативный hinter
    // FT_LOAD_NO_HINTING и FT_LOAD_FORCE_AUTOHINT игнорируются
}
```

## Кернинг

Кернинг — межсимвольный интервал для конкретных пар глифов.

### Проверка наличия

```cpp
if (FT_HAS_KERNING(face)) {
    // Шрифт содержит данные кернинга
}
```

### Получение кернинга

```cpp
FT_Vector kerning;
FT_Get_Kerning(
    face,
    left_glyph_index,
    right_glyph_index,
    FT_KERNING_DEFAULT,   // Режим
    &kerning
);

// Применение
pen_x += kerning.x;
pen_y += kerning.y;
```

### Режимы кернинга

| Режим                 | Описание                  |
|-----------------------|---------------------------|
| `FT_KERNING_DEFAULT`  | Grid-fitted, 26.6 пиксели |
| `FT_KERNING_UNFITTED` | Un-fitted, 26.6 пиксели   |
| `FT_KERNING_UNSCALED` | Font units                |

## Рендеринг

### Режимы рендеринга

```cpp
typedef enum FT_Render_Mode_ {
    FT_RENDER_MODE_NORMAL = 0,  // 8-bit grayscale
    FT_RENDER_MODE_LIGHT,       // То же что NORMAL
    FT_RENDER_MODE_MONO,        // 1-bit monochrome
    FT_RENDER_MODE_LCD,         // Horizontal LCD subpixel
    FT_RENDER_MODE_LCD_V,       // Vertical LCD subpixel
    FT_RENDER_MODE_SDF          // Signed Distance Field
} FT_Render_Mode;
```

### FT_Bitmap

```cpp
typedef struct FT_Bitmap_ {
    unsigned int    rows;       // Количество строк
    unsigned int    width;      // Количество столбцов
    int             pitch;      // Байт на строку (может быть < 0)
    unsigned char*  buffer;     // Данные пикселей
    unsigned short  num_grays;  // Уровней серого (обычно 256)
    unsigned char   pixel_mode; // FT_PIXEL_MODE_XXX
    unsigned char   palette_mode;
    void*           palette;
} FT_Bitmap;
```

### Pixel modes

```cpp
FT_PIXEL_MODE_NONE = 0
FT_PIXEL_MODE_MONO  = 1   // 1-bit
FT_PIXEL_MODE_GRAY  = 2   // 8-bit grayscale
FT_PIXEL_MODE_GRAY2 = 3   // 2-bit
FT_PIXEL_MODE_GRAY4 = 4   // 4-bit
FT_PIXEL_MODE_LCD   = 5   // Horizontal RGB
FT_PIXEL_MODE_LCD_V = 6   // Vertical RGB
FT_PIXEL_MODE_BGRA  = 7   // BGRA (color fonts)
```

## Outline

Векторный контур глифа доступен до растеризации.

### Структура FT_Outline

```cpp
typedef struct FT_Outline_ {
    FT_Short    n_contours;   // Количество контуров
    FT_Short    n_points;     // Количество точек
    FT_Vector*  points;       // Координаты точек
    unsigned char* tags;      // Типы точек
    int*        contours;     // Индексы последних точек контуров
    int         flags;
} FT_Outline;
```

### Теги точек

```cpp
FT_CURVE_TAG_ON      = 1   // On-curve point
FT_CURVE_TAG_CONIC   = 0   // Off-curve, quadratic Bezier
FT_CURVE_TAG_CUBIC   = 2   // Off-curve, cubic Bezier
```

### Декомпозиция контура

```cpp
int move_to(const FT_Vector* to, void* user) {
    // Начало нового контура
    return 0;
}

int line_to(const FT_Vector* to, void* user) {
    // Линия
    return 0;
}

int conic_to(const FT_Vector* control, const FT_Vector* to, void* user) {
    // Квадратичная кривая Безье
    return 0;
}

int cubic_to(const FT_Vector* control1, const FT_Vector* control2,
             const FT_Vector* to, void* user) {
    // Кубическая кривая Безье
    return 0;
}

FT_Outline_Funcs funcs = {
    move_to,
    line_to,
    conic_to,
    cubic_to,
    0,    // shift
    0     // delta
};

FT_Outline_Decompose(&face->glyph->outline, &funcs, user_data);
