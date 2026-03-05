# Инструменты FreeType

🟢 **Уровень 1: Начинающий**

FreeType включает утилиты для визуализации, отладки и анализа шрифтов. Находятся в директории `src/tools/` исходников.

## ftview

Визуализация глифов шрифта.

```bash
ftview [options] pt fontfile [indices]
```

### Основные параметры

| Параметр | Описание                        |
|----------|---------------------------------|
| `-e`     | Кодировка (например, `-e unic`) |
| `-f`     | Индекс face                     |
| `-r`     | DPI (по умолчанию 72)           |
| `-H`     | Режим hinting (0-2)             |
| `-l`     | Режим LCD                       |
| `-m`     | Отобразить метрики              |
| `-v`     | Версия                          |

### Примеры

```bash
# Просмотр шрифта размером 16pt
ftview 16 font.ttf

# Просмотр с LCD-рендерингом
ftview -l 1 24 font.ttf

# Конкретные глифы
ftview 48 font.ttf 65 66 67
```

## ftdiff

Сравнение рендеринга между разными версиями или режимами.

```bash
ftdiff [options] fontfile
```

### Основные параметры

| Параметр | Описание                |
|----------|-------------------------|
| `-f`     | Первый режим рендеринга |
| `-s`     | Второй режим рендеринга |
| `-H`     | Режим hinting           |

### Режимы рендеринга

```
0 - Normal
1 - Light
2 - Mono
3 - LCD
4 - LCD vertical
```

### Примеры

```bash
# Сравнение normal vs light hinting
ftdiff -f 0 -s 1 font.ttf

# Сравнение монохромного и grayscale
ftdiff -f 2 -s 0 font.ttf
```

## ftgrid

Отображение контуров глифов с сеткой.

```bash
ftgrid [options] pt fontfile [indices]
```

### Основные параметры

| Параметр | Описание         |
|----------|------------------|
| `-r`     | DPI              |
| `-f`     | Индекс face      |
| `-H`     | Режим hinting    |
| `-o`     | Показать outline |
| `-c`     | Показать контуры |

### Примеры

```bash
# Контур глифа 'A' (код 65)
ftgrid 48 font.ttf 65

# С сеткой и контурами
ftgrid -o -c 72 font.ttf
```

## ftinspect

Графический инструмент для инспекции шрифтов (Qt-based).

```bash
ftinspect [fontfile]
```

Возможности:

- Просмотр всех глифов
- Изменение размера в реальном времени
- Переключение режимов hinting
- Просмотр метрик
- Просмотр таблиц TrueType

## Официальные инструменты

### ftdump

Утилита для вывода информации о шрифте (встроена в ftview).

```bash
ftview -m font.ttf
```

### apinames

Генерация списка экспортируемых символов (для Windows DLL).

```bash
apinames freetype.dll
```

## Сторонние инструменты

### fonttools

Python-библиотека для работы со шрифтами:

```bash
pip install fonttools

# Вывод таблиц шрифта
ttx -t cmap font.ttf

# Дамп всех таблиц
ttx font.ttf
```

### HarfBuzz

Для продвинутого text shaping:

```bash
hb-view --font-file font.ttf --text "Hello"
```

### Microsoft Font Validator

Проверка соответствия спецификации OpenType:

```bash
FontValidator.exe font.ttf
```

## Анализ производительности

### Benchmarking

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

for (int i = 0; i < 1000; i++) {
    FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
}

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
```

### Memory tracking

```cpp
// Кастомный аллокатор для отслеживания
static size_t total_allocated = 0;

static void* tracking_alloc(FT_Memory memory, long size) {
    total_allocated += size;
    return malloc(size);
}

static void tracking_free(FT_Memory memory, void* block) {
    // Нужно отслеживать размер для точного подсчёта
    free(block);
}
```

## Отладка

### FT_DEBUG_LEVEL_TRACE

Скомпилируйте с `-DFT_DEBUG_LEVEL_TRACE` для включения логирования:

```bash
cmake -DCMAKE_C_FLAGS="-DFT_DEBUG_LEVEL_TRACE" ..
```

### FT_DEBUG_MEMORY

Для отслеживания утечек памяти:

```bash
cmake -DCMAKE_C_FLAGS="-DFT_DEBUG_MEMORY" ..
```

### Переменные окружения

```bash
# Управление свойствами драйверов
export FREETYPE_PROPERTIES="truetype:interpreter-version=35"

# Уровень логирования
export FT2_DEBUG="any:7"
```

## Примеры из исходников

В директории `examples/` исходников FreeType:

| Файл         | Описание                      |
|--------------|-------------------------------|
| `example1.c` | Базовый рендеринг глифа       |
| `example2.c` | Рендеринг строки              |
| `example3.c` | Конвертация в OpenGL текстуру |
