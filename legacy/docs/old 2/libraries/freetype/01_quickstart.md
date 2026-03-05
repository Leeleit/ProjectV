# Быстрый старт FreeType

🟢 **Уровень 1: Начинающий**

Минимальный пример рендеринга глифа: инициализация библиотеки, загрузка шрифта, получение глифа по коду символа,
рендеринг в битмап.

## Включение заголовков

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
```

Файл `ft2build.h` должен включаться первым — он определяет пути к остальным заголовкам.

## Инициализация библиотеки

```cpp
FT_Library library = nullptr;

FT_Error error = FT_Init_FreeType(&library);
if (error) {
    // Ошибка инициализации
    return -1;
}
```

`FT_Init_FreeType` создаёт корневой объект библиотеки. Вызывается один раз при старте приложения.

## Загрузка шрифта

```cpp
FT_Face face = nullptr;

error = FT_New_Face(
    library,
    "path/to/font.ttf",  // Путь к файлу шрифта
    0,                    // Индекс face (0 для первого)
    &face
);

if (error == FT_Err_Unknown_File_Format) {
    // Формат не поддерживается
} else if (error) {
    // Другая ошибка загрузки
}
```

Для загрузки из памяти:

```cpp
error = FT_New_Memory_Face(
    library,
    font_data,      // const FT_Byte* (указатель на данные)
    font_size,      // FT_Long (размер в байтах)
    0,              // face_index
    &face
);
```

## Установка размера

```cpp
// Установка размера в точках (pt)
error = FT_Set_Char_Size(
    face,
    0,              // char_width (0 = равен char_height)
    16 * 64,        // char_height в 26.6 формате (16pt)
    96,             // horz_resolution (dpi)
    96              // vert_resolution (dpi)
);

// Или в пикселях
error = FT_Set_Pixel_Sizes(
    face,
    0,              // pixel_width (0 = равен pixel_height)
    16              // pixel_height
);
```

## Получение индекса глифа

```cpp
FT_UInt glyph_index = FT_Get_Char_Index(face, 'A');

if (glyph_index == 0) {
    // Символ не найден в шрифте
}
```

FreeType использует индексы глифов, а не коды символов. Функция `FT_Get_Char_Index` преобразует код символа в индекс
через текущую charmap.

## Загрузка глифа

```cpp
error = FT_Load_Glyph(
    face,
    glyph_index,
    FT_LOAD_DEFAULT   // Флаги загрузки
);
```

Глиф загружается в `face->glyph` (FT_GlyphSlot).

## Рендеринг в битмап

```cpp
error = FT_Render_Glyph(
    face->glyph,
    FT_RENDER_MODE_NORMAL   // Режим рендеринга
);
```

Результат доступен в `face->glyph->bitmap`.

## Доступ к данным битмапа

```cpp
FT_Bitmap& bitmap = face->glyph->bitmap;

int width  = bitmap.width;
int height = bitmap.rows;
int pitch  = bitmap.pitch;        // Байт на строку (может быть отрицательным)
unsigned char* buffer = bitmap.buffer;  // Градации серого (0-255)

// Позиция битмапа относительно baseline
int left = face->glyph->bitmap_left;
int top  = face->glyph->bitmap_top;

// Advance для следующего глифа
FT_Pos advance_x = face->glyph->advance.x;  // В формате 26.6
```

## Полный пример

```cpp
#include <ft2build.h>
#include <freetype/freetype.h>
#include <cstdio>

int main() {
    FT_Library library = nullptr;
    FT_Face face = nullptr;

    // 1. Инициализация
    FT_Error error = FT_Init_FreeType(&library);
    if (error) {
        printf("Failed to initialize FreeType\n");
        return 1;
    }

    // 2. Загрузка шрифта
    error = FT_New_Face(library, "font.ttf", 0, &face);
    if (error) {
        printf("Failed to load font\n");
        FT_Done_FreeType(library);
        return 1;
    }

    // 3. Установка размера
    error = FT_Set_Pixel_Sizes(face, 0, 48);
    if (error) {
        printf("Failed to set size\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    // 4. Загрузка символа 'A'
    FT_UInt glyph_index = FT_Get_Char_Index(face, 'A');
    if (glyph_index == 0) {
        printf("Character not found\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    // 5. Загрузка и рендеринг
    error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    if (error) {
        printf("Failed to load glyph\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {
        printf("Failed to render glyph\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    // 6. Вывод информации
    FT_Bitmap& bitmap = face->glyph->bitmap;
    printf("Glyph: %dx%d pixels\n", bitmap.width, bitmap.rows);
    printf("Position: left=%d, top=%d\n",
           face->glyph->bitmap_left,
           face->glyph->bitmap_top);

    // 7. ASCII-визуализация
    for (int y = 0; y < bitmap.rows; y++) {
        for (int x = 0; x < bitmap.width; x++) {
            unsigned char pixel = bitmap.buffer[y * bitmap.pitch + x];
            char c = (pixel > 128) ? '#' : (pixel > 64) ? '+' : ' ';
            putchar(c);
        }
        putchar('\n');
    }

    // 8. Освобождение ресурсов
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    return 0;
}
```

## Рендеринг строки

```cpp
void render_string(FT_Face face, const char* text, int start_x, int start_y) {
    FT_Pos pen_x = start_x << 6;  // Конвертация в 26.6
    FT_Pos pen_y = start_y << 6;

    for (const char* p = text; *p; p++) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, *p);

        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        if (error) continue;

        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        if (error) continue;

        FT_Bitmap& bitmap = face->glyph->bitmap;

        // Позиция для отрисовки
        int x = (pen_x >> 6) + face->glyph->bitmap_left;
        int y = (pen_y >> 6) - face->glyph->bitmap_top;

        // Отрисовка bitmap в целевой буфер
        draw_bitmap(&bitmap, x, y);

        // Продвижение пера
        pen_x += face->glyph->advance.x;
        pen_y += face->glyph->advance.y;
    }
}
```

## Освобождение ресурсов

```cpp
FT_Done_Face(face);        // Освобождает face и все его sizes/slots
FT_Done_FreeType(library); // Освобождает library и все ресурсы
```

Порядок важен: сначала освобождаются face, затем library.

## Обработка ошибок

FreeType возвращает код ошибки типа `FT_Error`. Значение `0` означает успех.

```cpp
#define FT_ERR_PREFIX  FT_Err_
#define FT_ERR_DEF( e, v, s )  case v: return s;

const char* get_error_string(FT_Error error) {
    switch (error) {
        #include <freetype/fterrdef.h>
        default: return "unknown error";
    }
}
```

Или используйте `FT_Error_String` (доступно с версии 2.10):

```cpp
const char* msg = FT_Error_String(error);
if (msg) {
    printf("Error: %s\n", msg);
}
