# Устранение неполадок FreeType

🟡 **Уровень 2: Средний**

Типичные проблемы при работе с FreeType и их решения.

## Ошибки загрузки шрифтов

### FT_Err_Unknown_File_Format

**Проблема**: Формат файла не поддерживается.

**Причины**:

- Файл повреждён
- Формат не поддерживается (например, EOT)
- Файл пустой

**Решение**:

```cpp
FT_Error error = FT_New_Face(library, "font.ttf", 0, &face);
if (error == FT_Err_Unknown_File_Format) {
    // Проверить файл
    FILE* f = fopen("font.ttf", "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        if (size == 0) {
            printf("File is empty\n");
        }
    }
}
```

### FT_Err_Cannot_Open_Resource

**Проблема**: Файл не найден или нет доступа.

**Решение**:

```cpp
// Проверка существования
#include <sys/stat.h>

bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// На Windows с Unicode путями
#ifdef _WIN32
#include <windows.h>
std::wstring utf8_to_wide(const std::string& utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], len);
    return wide;
}

// Чтение файла и загрузка из памяти
HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, 0, nullptr);
// ... read and use FT_New_Memory_Face
#endif
```

### FT_Err_Invalid_Argument

**Проблема**: Неверный индекс face или аргумент.

**Причины**:

- `face_index` больше `num_faces`
- Неверный `FT_Open_Args`

**Решение**:

```cpp
// Проверка количества faces
FT_Face temp_face;
FT_Error error = FT_New_Face(library, "font.ttc", -1, &temp_face);
if (!error) {
    FT_Long num_faces = temp_face->num_faces;
    FT_Done_Face(temp_face);

    for (FT_Long i = 0; i < num_faces; i++) {
        FT_New_Face(library, "font.ttc", i, &face);
        // ...
    }
}
```

## Проблемы с кодировками

### Символ не найден (glyph_index = 0)

**Проблема**: `FT_Get_Char_Index` возвращает 0.

**Причины**:

- Нет Unicode charmap
- Символ отсутствует в шрифте
- Неверная кодировка

**Решение**:

```cpp
// Проверка доступных charmaps
for (FT_Int i = 0; i < face->num_charmaps; i++) {
    FT_CharMap cmap = face->charmaps[i];
    printf("Charmap %d: platform=%d, encoding=%d\n",
           i, cmap->platform_id, cmap->encoding_id);
}

// Выбор Unicode charmap
FT_Error error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
if (error) {
    // Попробовать другие
    for (FT_Int i = 0; i < face->num_charmaps; i++) {
        if (face->charmaps[i]->encoding == FT_ENCODING_APPLE_ROMAN) {
            FT_Set_Charmap(face, face->charmaps[i]);
            break;
        }
    }
}
```

### Неправильные символы

**Проблема**: Отображаются не те символы.

**Причины**:

- Неверная кодировка входного текста
- Неправильный charmap

**Решение**:

```cpp
// Убедиться в Unicode
const char* text = u8"Привет";  // UTF-8

// Конвертация UTF-8 в UTF-32
#include <codecvt>
std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
std::u32string utf32 = converter.from_bytes(text);

for (char32_t c : utf32) {
    FT_UInt glyph_index = FT_Get_Char_Index(face, c);
    // ...
}
```

## Проблемы рендеринга

### Пустой битмап

**Проблема**: `bitmap.width` или `bitmap.rows` равны 0.

**Причины**:

- Пробельный символ (space)
- Glyph не рендерился

**Решение**:

```cpp
FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);

// Для пробельных символов advance работает, но битмап пустой
if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
}

FT_Bitmap& bitmap = face->glyph->bitmap;
if (bitmap.width == 0 || bitmap.rows == 0) {
    // Использовать advance для позиционирования
    pen_x += face->glyph->advance.x;
    continue;
}
```

### Нечитаемый текст

**Проблема**: Текст размытый или слишком мелкий.

**Причины**:

- Неправильный размер
- Неподходящий режим hinting

**Решение**:

```cpp
// Увеличить DPI для мобильных устройств
FT_Set_Char_Size(face, 0, 16 * 64, 144, 144);  // 144 DPI

// Попробовать разные режимы
FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_LIGHT);  // Для UI
FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_NORMAL); // Для текста
FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_MONO);   // Для печати
```

### Обрезанные глифы

**Проблема**: Части глифов обрезаны.

**Причины**:

- Неправильный расчёт bounding box
- Игнорирование negative bearing

**Решение**:

```cpp
// Правильный расчёт размеров
int min_x = INT_MAX, min_y = INT_MAX;
int max_x = INT_MIN, max_y = INT_MIN;

for (each glyph) {
    int x = pen_x + slot->bitmap_left;
    int y = pen_y - slot->bitmap_top;

    min_x = std::min(min_x, x);
    min_y = std::min(min_y, y);
    max_x = std::max(max_x, x + (int)slot->bitmap.width);
    max_y = std::max(max_y, y + (int)slot->bitmap.rows);

    pen_x += slot->advance.x >> 6;
}

int width = max_x - min_x;
int height = max_y - min_y;
```

## Проблемы с памятью

### Утечка памяти

**Проблема**: Память растёт при загрузке шрифтов.

**Причины**:

- Забыт `FT_Done_Face`
- Забыт `FT_Done_Glyph`
- Незакрытый `FT_New_Memory_Face`

**Решение**:

```cpp
// RAII wrapper
class Face {
    FT_Face face_ = nullptr;
public:
    Face(FT_Library lib, const char* path) {
        FT_New_Face(lib, path, 0, &face_);
    }
    ~Face() {
        if (face_) FT_Done_Face(face_);
    }
    Face(const Face&) = delete;
    Face& operator=(const Face&) = delete;
    FT_Face get() const { return face_; }
};
```

### FT_Err_Out_Of_Memory

**Проблема**: FreeType не может выделить память.

**Причины**:

- Слишком большой шрифт
- Повреждённый шрифт с огромными значениями

**Решение**:

```cpp
// Ограничить максимальный размер
const size_t MAX_FONT_SIZE = 50 * 1024 * 1024;  // 50MB

FILE* f = fopen(path, "rb");
fseek(f, 0, SEEK_END);
long size = ftell(f);
fclose(f);

if (size > MAX_FONT_SIZE) {
    printf("Font too large: %ld bytes\n", size);
    return false;
}
```

## Проблемы многопоточности

### Краш при загрузке

**Проблема**: Segfault при одновременной загрузке шрифтов.

**Причины**:

- Одновременный доступ к одному `FT_Library`

**Решение**:

```cpp
// Вариант 1: один library на поток
thread_local FT_Library tl_library;

// Вариант 2: mutex для создания/удаления face
std::mutex g_ft_mutex;

FT_Face load_face(FT_Library lib, const char* path) {
    std::lock_guard<std::mutex> lock(g_ft_mutex);
    FT_Face face;
    FT_New_Face(lib, path, 0, &face);
    return face;
}
```

## Tricky fonts

### Игнорируются флаги hinting

**Проблема**: `FT_LOAD_NO_HINTING` не работает.

**Причины**:

- Шрифт в списке "tricky fonts"

**Решение**:

```cpp
if (FT_IS_TRICKY(face)) {
    // Эти шрифты требуют нативного hinting
    // FT_LOAD_NO_HINTING будет проигнорирован

    // Использовать нативный hinter
    FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
}
```

### Список tricky fonts

Некоторые старые китайские шрифты:

- `mingli.ttf` (но не `mingliu.ttc`)
- И другие (~12 шрифтов)

Определены в `ttobjs.c` исходников FreeType.

## Отладка

### Включение логирования

```cpp
// При компиляции
cmake -DCMAKE_C_FLAGS="-DFT_DEBUG_LEVEL_TRACE" ..

// Или установка переменной окружения
set FT2_DEBUG=any:7
```

### Вывод информации о шрифте

```cpp
void print_face_info(FT_Face face) {
    printf("Family: %s\n", face->family_name);
    printf("Style: %s\n", face->style_name);
    printf("Glyphs: %ld\n", face->num_glyphs);
    printf("Units per EM: %d\n", face->units_per_EM);
    printf("Ascender: %d\n", face->ascender);
    printf("Descender: %d\n", face->descender);
    printf("Height: %d\n", face->height);

    printf("Flags:\n");
    if (FT_IS_SCALABLE(face)) printf("  Scalable\n");
    if (FT_IS_FIXED_WIDTH(face)) printf("  Fixed width\n");
    if (FT_HAS_KERNING(face)) printf("  Kerning\n");
    if (FT_HAS_GLYPH_NAMES(face)) printf("  Glyph names\n");
    if (FT_IS_TRICKY(face)) printf("  Tricky\n");
}
```

### Проверка ошибки

```cpp
const char* error_string(FT_Error error) {
    #undef __FTERRORS_H__
    #define FT_ERRORDEF(e, v, s) case e: return s;
    #define FT_ERROR_START_LIST switch (error) {
    #define FT_ERROR_END_LIST }
    #include <freetype/fterrors.h>
    return "unknown error";
}
```

## Коды ошибок

| Код  | Имя                           | Описание                |
|------|-------------------------------|-------------------------|
| 0x01 | `FT_Err_Cannot_Open_Resource` | Не удалось открыть файл |
| 0x02 | `FT_Err_Unknown_File_Format`  | Неизвестный формат      |
| 0x03 | `FT_Err_Invalid_File_Format`  | Повреждённый файл       |
| 0x10 | `FT_Err_Invalid_Argument`     | Неверный аргумент       |
| 0x17 | `FT_Err_Invalid_Glyph_Index`  | Неверный индекс глифа   |
| 0x22 | `FT_Err_Invalid_Pixel_Size`   | Неверный размер пикселя |
| 0x40 | `FT_Err_Invalid_Handle`       | Неверный handle         |
| 0x41 | `FT_Err_Out_Of_Memory`        | Нехватка памяти         |
