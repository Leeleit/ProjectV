## Обзор FreeType

<!-- anchor: 00_overview -->

🟢 **Уровень 1: Начинающий**

**FreeType** — библиотека для рендеринга шрифтов на C. Предоставляет низкоуровневый API для загрузки шрифтов,
масштабирования глифов и растеризации в битмапы. Не является text layout engine — для сложного текста требуется
HarfBuzz, Pango или ICU.

Версия: **2.14.1**
Исходники: [freetype/freetype](https://gitlab.freedesktop.org/freetype/freetype)
Документация: [freetype.org/freetype2/docs](https://freetype.org/freetype2/docs/)

## Основные возможности

- **Загрузка шрифтов** — TrueType, OpenType, Type 1, CFF, CID, bitmap fonts, WOFF
- **Масштабирование** — векторные контуры в любые размеры
- **Растеризация** — антиалиасинг, subpixel rendering (LCD), монохром
- **Hinting** — нативный (TrueType bytecode) и auto-hinting
- **Кернинг** — извлечение из 'kern' таблицы и GPOS
- **Метрики** — advance width, bearings, bounding box
- **Вариации** — OpenType Font Variations, Multiple Masters

## Типичное применение

- Рендеринг текста в играх и приложениях
- Font preview и управление шрифтами
- Конвертация шрифтов между форматами
- Встраивание в UI библиотеки (RmlUi, ImGui)
- PDF и document rendering

## Архитектура

Иерархия объектов FreeType:

```
FT_Library
    └── FT_Face (шрифт)
            ├── FT_Size (размер)
            ├── FT_GlyphSlot (текущий глиф)
            └── FT_CharMap[] (таблицы символов)
```

| Объект           | Назначение                            | Жизненный цикл                    |
|------------------|---------------------------------------|-----------------------------------|
| **FT_Library**   | Корневой контекст, управляет модулями | Создаётся один раз при старте     |
| **FT_Face**      | Загруженный шрифт                     | Создаётся на каждый шрифт         |
| **FT_Size**      | Размер шрифта                         | Создаётся при изменении размера   |
| **FT_GlyphSlot** | Контейнер для глифа                   | Один на FT_Face, переиспользуется |
| **FT_CharMap**   | Таблица символов                      | Несколько на FT_Face              |

## Карта заголовков

| Заголовок               | Содержимое                                |
|-------------------------|-------------------------------------------|
| `<ft2build.h>`          | Точка входа (включить первым)             |
| `<freetype/freetype.h>` | Главный API: init, face, glyph, charmap   |
| `<freetype/ftglyph.h>`  | Работа с глифами: FT_Glyph, FT_Glyph_Copy |
| `<freetype/ftoutln.h>`  | Контуры: FT_Outline, FT_Outline_Decompose |
| `<freetype/ftbitmap.h>` | Битмапы: FT_Bitmap, FT_Bitmap_Convert     |
| `<freetype/ftcache.h>`  | Кэширование: FTC_Manager, FTC_CMapCache   |
| `<freetype/ftstroke.h>` | Обводка: FT_Stroker                       |
| `<freetype/ftsynth.h>`  | Синтез: FT_GlyphSlot_Embolden, Oblique    |
| `<freetype/ftrender.h>` | Рендереры: FT_Renderer                    |
| `<freetype/ftmodapi.h>` | Модули: FT_Add_Module, FT_Property_Set    |
| `<freetype/ftsystem.h>` | Система: память, потоки                   |

## Форматы шрифтов

| Формат   | Расширения             | Особенности                          |
|----------|------------------------|--------------------------------------|
| TrueType | `.ttf`, `.ttc`         | Bytecode hinting, glyf table         |
| OpenType | `.otf`, `.ttf`         | CFF или TrueType outlines, GPOS/GSUB |
| Type 1   | `.pfa`, `.pfb`         | PostScript outlines, AFM metrics     |
| CFF      | `.cff`                 | Compact Font Format                  |
| CID      | `.cid`                 | CID-keyed fonts                      |
| Bitmap   | `.pcf`, `.bdf`, `.fon` | Растровые шрифты                     |
| WOFF     | `.woff`, `.woff2`      | Сжатые веб-шрифты                    |

## Требования

- **ANSI C** компилятор (C99+)
- **Стандартная библиотека** C
- Для сборки: CMake, Meson или configure/make

## Содержание документации

| Раздел                                         | Описание                                      |
|------------------------------------------------|-----------------------------------------------|
| [01_quickstart.md](01_quickstart.md)           | Минимальный пример: init → load → render      |
| [02_concepts.md](02_concepts.md)               | FT_Library, FT_Face, фиксированная точка 26.6 |
| [03_integration.md](03_integration.md)         | CMake, зависимости, платформы                 |
| [04_api-reference.md](04_api-reference.md)     | Справочник функций и структур                 |
| [05_tools.md](05_tools.md)                     | ftview, ftdiff, ftgrid                        |
| [06_performance.md](06_performance.md)         | Кэширование, batch rendering, memory          |
| [07_troubleshooting.md](07_troubleshooting.md) | Ошибки, кодировки, битые шрифты               |
| [08_glossary.md](08_glossary.md)               | Словарь терминов                              |

## Для ProjectV

Интеграция с ProjectV (Vulkan, SDL3, VMA, RmlUi) описана в отдельных файлах:

- [09_projectv-integration.md](09_projectv-integration.md) — связка с Vulkan рендерером и RmlUi
- [10_projectv-patterns.md](10_projectv-patterns.md) — паттерны FontCache, GlyphAtlas, TextRenderer

---

## Концепции FreeType

<!-- anchor: 02_concepts -->

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

---

## Глоссарий FreeType

<!-- anchor: 08_glossary -->

🟢 **Уровень 1: Начинающий**

Термины и понятия, используемые в FreeType и типографике.

## Основные термины

### Glyph (Глиф)

Графическое представление символа. Один символ может иметь несколько глифов (например, начертания: обычное, курсивное, капитель). Глиф может не соответствовать одному символу (лигатуры, диакритические знаки).

### Glyph Index

Уникальный числовой идентификатор глифа в шрифте. Не совпадает с Unicode code point. Преобразование charcode → glyph_index выполняется через charmap.

### Face

Загруженный экземпляр шрифта. Face содержит все данные о конкретном начертании шрифта (regular, bold, italic и т.д.).

### Charmap (Character Map)

Таблица соответствия между кодами символов и индексами глифов. Шрифт может содержать несколько charmap (Unicode, Symbol, Apple Roman и др.).

## Метрики

### EM Square (EM-square, UPM)

Виртуальный квадрат, в котором рисуются контуры глифов. Размер `units_per_EM` — количество единиц на сторону квадрата. Обычно 2048 (TrueType) или 1000 (Type 1, CFF).

### Advance Width (Advance)

Расстояние от текущей позиции пера до позиции для следующего глифа по горизонтали. Включает ширину глифа и межсимвольный интервал.

### Advance Height

Аналог Advance Width для вертикального письма.

### Bearing

Смещение от точки привязки (origin) до края глифа.

- **Left Bearing (horiBearingX)**: Горизонтальное смещение от origin до левого края.
- **Top Bearing (horiBearingY)**: Вертикальное смещение от baseline до верхнего края.

### Baseline

Линия, на которой "стоят" символы. Разные шрифты могут иметь разные baseline (alphabetic, hanging, ideographic).

### Ascender

Расстояние от baseline до верхней линии шрифта (верхняя граница прописных букв и высоких строчных).

### Descender

Расстояние от baseline до нижней линии шрифта (нижняя граница хвостиков букв g, y, p). Обычно отрицательное значение.

### Line Height (Line Gap)

Расстояние между baseline соседних строк. Полная высота строки = ascender - descender + line gap.

### Bounding Box (BBox)

Прямоугольник, ограничивающий видимую часть глифа или всего шрифта.

### Kerning

Корректировка межсимвольного интервала для конкретных пар глифов. Улучшает визуальное восприятие текста (например, уменьшение расстояния между "AV").

## Рендеринг

### Rasterization (Растеризация)

Преобразование векторного контура в растровое изображение (битмап).

### Hinting

Процесс подгонки контуров глифов к пиксельной сетке для улучшения читаемости при малых размерах.

- **Native hinting**: Использование инструкций, зашитых в шрифт (TrueType bytecode).
- **Auto-hinting**: Автоматическое hinting без использования инструкций шрифта.

### Anti-aliasing

Сглаживание краёв растрированного изображения для устранения "лесенки". В FreeType реализуется через градации серого (8-bit grayscale).

### Subpixel Rendering

Метод рендеринга, использующий физическую структуру LCD-панелей (RGB subpixels) для увеличения эффективного разрешения по горизонтали в 3 раза. Требует специального фильтра (LCD filter).

### SDF (Signed Distance Field)

Представление контура через карту расстояний. Каждое значение — расстояние до ближайшей границы контура (положительное снаружи, отрицательное внутри). Используется для качественного масштабирования текста в 3D.

## Контур

### Outline

Векторное описание формы глифа. Состоит из контуров (contours) — замкнутых кривых. Каждый контур состоит из сегментов: линий и кривых Безье.

### Contour

Замкнутый путь в outline. Простой глиф (буква "O") имеет один контур. Сложный глиф (буква "B") — несколько контуров.

### On-curve Point

Точка, лежащая на контуре. Через неё проходит видимая линия.

### Off-curve Point

Контрольная точка кривой Безье. Не лежит на контуре, но определяет его форму.

### Bezier Curve

Параметрическая кривая:

- **Quadratic (квадратичная)**: 1 control point, используется в TrueType.
- **Cubic (кубическая)**: 2 control points, используется в Type 1, CFF, OpenType CFF.

## Форматы

### TrueType

Формат шрифтов от Apple/Microsoft. Характеристики:
- Quadratic Bezier curves
- Bytecode hinting
- Файлы: `.ttf`, `.ttc` (TrueType Collection)

### OpenType

Формат от Microsoft/Adobe, надстройка над TrueType или CFF. Характеристики:
- Поддержка TrueType или CFF outlines
- Расширенная типографика (GPOS, GSUB)
- Файлы: `.otf`, `.ttf`

### CFF (Compact Font Format)

Формат от Adobe, основан на Type 2. Характеристики:
- Cubic Bezier curves
- Компактное хранение
- Используется в OpenType

### Type 1

PostScript шрифт от Adobe. Характеристики:
- Cubic Bezier curves
- Отдельные файлы для метрик (AFM, PFM)
- Устаревший формат
- Файлы: `.pfa`, `.pfb`

### WOFF / WOFF2

Web Open Font Format — сжатые форматы для веба. WOFF2 использует Brotli сжатие.

## Структуры FreeType

### FT_Library

Корневой объект библиотеки. Управляет модулями и ресурсами.

### FT_Face

Объект загруженного шрифта. Содержит все данные о начертании.

### FT_Size

Объект размера шрифта. Хранит текущий масштаб и метрики для конкретного размера.

### FT_GlyphSlot

Контейнер для текущего загруженного глифа. Переиспользуется при каждой загрузке.

### FT_Bitmap

Растровое изображение глифа. Содержит ширину, высоту, pitch и пиксельные данные.

### FT_Outline

Векторное описание контура глифа. Массив точек и тегов.

## Форматы чисел

### 26.6 Fixed-Point

Формат с фиксированной точкой: 26 бит целая часть, 6 бит дробная. Единица = 1/64 пикселя. Используется для координат и размеров.

### 16.16 Fixed-Point

16 бит целая часть, 16 бит дробная. Используется для коэффициентов масштабирования.

## Прочее

### PPEM (Pixels Per EM)

Размер шрифта в пикселях, измеренный как количество пикселей на сторону EM-квадрата.

### Strike

Набор растровых глифов для конкретного размера. Bitmap fonts или embedded bitmaps в векторных шрифтах.

### Ligature

Глиф, объединяющий несколько символов (например, "fi", "fl", "ffi").

### Variation Font

Шрифт с настраиваемыми параметрами (weight, width, optical size). OpenType Font Variations или Multiple Masters.
