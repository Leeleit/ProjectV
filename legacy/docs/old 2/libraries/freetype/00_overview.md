# Обзор FreeType

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
