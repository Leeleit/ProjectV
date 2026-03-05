# FreeType: Рендеринг шрифтов

> **Для понимания:** Представьте, что FreeType — это "переводчик" между математикой и пикселями. Шрифт — это набор
> математических формул (кривых Безье), а экран — сетка пикселей. FreeType берёт эти формулы, "подгоняет" их к
> пиксельной
> сетке (hinting), и превращает в чёткие символы даже при маленьких размерах. Без FreeType текст выглядел бы как
> размытые
> пятна.

**FreeType** — библиотека для рендеринга шрифтов на C. Предоставляет низкоуровневый API для загрузки шрифтов,
масштабирования глифов и растеризации в битмапы.

## Основные возможности

- **Загрузка шрифтов** — TrueType, OpenType, Type 1, CFF, CID, bitmap fonts, WOFF
- **Масштабирование** — векторные контуры в любые размеры
- **Растеризация** — антиалиасинг, subpixel rendering (LCD), монохром для pixel-perfect
- **Hinting** — нативный (TrueType bytecode) и auto-hinting
- **Кернинг** — извлечение из 'kern' таблицы и GPOS
- **Метрики** — advance width, bearings, bounding box

## Иерархия объектов

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

## Карта заголовков

| Заголовок               | Содержимое                                |
|-------------------------|-------------------------------------------|
| `<ft2build.h>`          | Точка входа (включить первым)             |
| `<freetype/freetype.h>` | Главный API: init, face, glyph, charmap   |
| `<freetype/ftglyph.h>`  | Работа с глифами: FT_Glyph, FT_Glyph_Copy |
| `<freetype/ftoutln.h>`  | Контуры: FT_Outline, FT_Outline_Decompose |
| `<freetype/ftbitmap.h>` | Битмапы: FT_Bitmap, FT_Bitmap_Convert     |
| `<freetype/ftcache.h>`  | Кэширование: FTC_Manager, FTC_CMapCache   |
| `<freetype/ftimage.h>`  | Структуры изображений: FT_Vector, FT_BBox |
| `<freetype/fttypes.h>`  | Основные типы: FT_Int, FT_Fixed, FT_Pos   |
| `<freetype/ftsystem.h>` | Кастомные аллокаторы и файловые системы   |
| `<freetype/ftrender.h>` | Рендереры: FT_Renderer, FT_Render_Mode    |
| `<freetype/ftstroke.h>` | Обводка контуров: FT_Stroker              |
| `<freetype/ftlcdfil.h>` | LCD фильтры для subpixel rendering        |
| `<freetype/ftsizes.h>`  | Управление размерами шрифтов              |
| `<freetype/ftmm.h>`     | Multiple Masters и вариативные шрифты     |
| `<freetype/ftadvanc.h>` | Расширенная работа с метриками            |

## Форматы шрифтов

| Формат   | Расширения             | Особенности                          |
|----------|------------------------|--------------------------------------|
| TrueType | `.ttf`, `.ttc`         | Bytecode hinting, glyf table         |
| OpenType | `.otf`, `.ttf`         | CFF или TrueType outlines, GPOS/GSUB |
| CFF      | `.cff`                 | Compact Font Format                  |
| Bitmap   | `.pcf`, `.bdf`, `.fon` | Растровые шрифты                     |
| WOFF     | `.woff`, `.woff2`      | Сжатые веб-шрифты                    |

## FT_Library

Корневой объект библиотеки:

```cpp
FT_Library library;
FT_Init_FreeType(&library);   // Создание
FT_Done_FreeType(library);    // Уничтожение
```

## FT_Face

Объект шрифта:

```cpp
FT_Face face;
FT_New_Face(library, "font.ttf", 0, &face);
FT_Done_Face(face);
```

Ключевые поля:

| Поле           | Тип          | Описание                            |
|----------------|--------------|-------------------------------------|
| `num_glyphs`   | FT_Long      | Количество глифов                   |
| `units_per_EM` | FT_UShort    | Единиц на EM (обычно 2048 или 1000) |
| `ascender`     | FT_Short     | Верхняя линия                       |
| `descender`    | FT_Short     | Нижняя линия (отрицательное)        |
| `glyph`        | FT_GlyphSlot | Текущий глиф                        |

## Фиксированная точка

FreeType использует форматы с фиксированной точкой.

### Формат 26.6

26 бит целая часть, 6 бит дробная. Единица = 1/64 пикселя.

```cpp
// Конвертация из int в 26.6
FT_Pos fixed = int_value << 6;

// Конвертация из 26.6 в int
int integer = fixed >> 6;

// Конвертация в float
float pixels = fixed / 64.0f;
```

## Загрузка и рендеринг глифа

```cpp
// Выбор Unicode charmap
FT_Select_Charmap(face, FT_ENCODING_UNICODE);

// Получение индекса глифа
FT_UInt glyph_index = FT_Get_Char_Index(face, 'A');

// Загрузка глифа
FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);

// Доступ к битмапу
FT_Bitmap& bitmap = face->glyph->bitmap;
// bitmap.rows, bitmap.width, bitmap.buffer
```

## Hinting

```cpp
FT_LOAD_TARGET_NORMAL  // Стандартный hinting
FT_LOAD_TARGET_LIGHT   // Лёгкий hinting
FT_LOAD_TARGET_MONO    // Монохромный
FT_LOAD_TARGET_LCD     // LCD-оптимизированный
```

## Кернинг

```cpp
if (FT_HAS_KERNING(face)) {
    FT_Vector kerning;
    FT_Get_Kerning(face, left_glyph, right_glyph,
                   FT_KERNING_DEFAULT, &kerning);
}
```

## Глоссарий

### Glyph (Глиф)

Графическое представление символа.

### EM Square

Виртуальный квадрат, в котором рисуются контуры глифов. Обычно 2048 (TrueType) или 1000 (Type 1).

### Baseline

Линия, на которой "стоят" символы.

### Ascender/Descender

Расстояние от baseline до верхней/нижней линии шрифта.

### Hinting

Процесс подгонки контуров к пиксельной сетке для лучшей читаемости.

### SDF (Signed Distance Field)

Представление контура через карту расстояний для качественного масштабирования.
