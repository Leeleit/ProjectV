# Установка Slang

**🟢 Уровень 1: Начинающий** — Способы установки Slang: Vulkan SDK, бинарные пакеты, сборка из исходников.

---

## Вариант A: Vulkan SDK (рекомендуется)

Slang включён в Vulkan SDK начиная с версии 1.3.296.0. Это рекомендуемый способ для большинства пользователей.

### Проверка установки

```bash
# Проверьте наличие slangc в PATH
slangc --version

# Если команда не найдена, убедитесь, что Vulkan SDK установлен
# и переменная VULKAN_SDK указывает на правильный путь
```

### Установка Vulkan SDK

Скачайте последнюю версию с официального сайта:

```
https://vulkan.lunarg.com/sdk/home
```

После установки проверьте переменную окружения:

```bash
# Linux/macOS
echo $VULKAN_SDK
# Ожидаемый вывод: /path/to/VulkanSDK/1.3.xxx.0/... или аналогичный

# Windows (PowerShell)
echo $env:VULKAN_SDK
```

---

## Вариант B: Бинарные пакеты

Если Vulkan SDK недоступен или нужна последняя версия Slang, скачайте бинарный пакет с GitHub Releases.

### Windows

```bash
# Скачивание
curl -LO https://github.com/shader-slang/slang/releases/latest/download/slang-win64-release.zip

# Распаковка
unzip slang-win64-release.zip -d C:/slang

# Добавление в PATH (PowerShell)
$env:PATH += ";C:\slang\bin"
```

### Linux

```bash
# Скачивание
curl -LO https://github.com/shader-slang/slang/releases/latest/download/slang-linux-x86_64-release.tar.gz

# Распаковка
tar -xzf slang-linux-x86_64-release.tar.gz -C /usr/local

# Обновление кэша библиотек
sudo ldconfig
```

### macOS

```bash
# Скачивание
curl -LO https://github.com/shader-slang/slang/releases/latest/download/slang-macos-release.zip

# Распаковка
unzip slang-macos-release.zip -d /usr/local
```

---

## Вариант C: Сборка из исходников

Для разработки или использования последних изменений.

### Требования для сборки

- **CMake 3.25+**
- **C++20 совместимый компилятор** (MSVC 2022, GCC 11+, Clang 14+)
- **Git** (для клонирования подмодулей)

### Клонирование репозитория

```bash
git clone https://github.com/shader-slang/slang.git
cd slang

# Инициализация подмодулей
git submodule update --init --recursive
```

### Конфигурация и сборка

```bash
# Создание директории сборки
mkdir build && cd build

# Конфигурация (Linux/macOS)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Конфигурация (Windows, Visual Studio 2022)
cmake .. -G "Visual Studio 17 2022" -A x64

# Сборка (параллельная, использует все ядра)
cmake --build . --config Release --parallel

# Установка (опционально)
cmake --install . --prefix /usr/local
```

### Сборка конкретных целей

```bash
# Только компилятор slangc
cmake --build . --config Release --target slangc

# Только библиотека Slang
cmake --build . --config Release --target slang

# Тесты
cmake --build . --config Release --target slang-test
```

---

## Интеграция с CMake

### find_package

```cmake
# Поиск установленного Slang
find_package(Slang REQUIRED)

# Использование
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE slang::slang)
```

### add_subdirectory

```cmake
# Если Slang включён как подмодуль проекта
add_subdirectory(external/slang)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE slang)
```

### Поиск slangc исполняемого файла

```cmake
# Найти slangc для компиляции шейдеров
find_program(SLANGC_EXECUTABLE slangc
    HINTS
        $ENV{VULKAN_SDK}/bin
        ${CMAKE_SOURCE_DIR}/external/slang/build/bin
)

if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc not found")
endif()
```

---

## Проверка установки

### Командная строка

```bash
# Проверка версии
slangc --version

# Пример вывода:
# slangc version 2025.x.x

# Тестовая компиляция
echo "[numthreads(1,1,1)] void main() {}" > test.slang
slangc test.slang -target spirv -o test.spv
rm test.slang test.spv
```

### CMake тест

```cmake
cmake_minimum_required(VERSION 3.25)
project(slang_test)

find_package(Slang REQUIRED)

add_executable(slang_test main.cpp)
target_link_libraries(slang_test PRIVATE slang::slang)
```

```cpp
// main.cpp
#include <slang.h>
#include <iostream>

int main() {
    Slang::ComPtr<slang::IGlobalSession> globalSession;
    SlangResult result = slang::createGlobalSession(globalSession.writeRef());
    
    if (SLANG_SUCCEEDED(result)) {
        std::cout << "Slang initialized successfully" << std::endl;
        return 0;
    } else {
        std::cerr << "Failed to initialize Slang" << std::endl;
        return 1;
    }
}
```

---

## Структура установки

После установки или сборки структура директорий:

```
slang/
├── bin/
│   ├── slangc           # Компилятор командной строки
│   ├── slangd           # Language daemon (LSP)
│   └── slang-test       # Тестовый раннер
├── lib/
│   ├── slang.so / slang.dll    # Основная библиотека
│   ├── slang-rt.so / slang-rt.dll  # Runtime библиотека
│   └── cmake/
│       └── slang/       # CMake config файлы
└── include/
    ├── slang.h          # Основной заголовок
    ├── slang-com-ptr.h  # ComPtr helper
    └── slang-gfx.h      # Graphics layer (опционально)
```

---

## Устранение неполадок

### slangc не найден

```bash
# Проверьте PATH
which slangc        # Linux/macOS
where slangc        # Windows

# Добавьте в PATH временно
export PATH=$PATH:/path/to/slang/bin  # Linux/macOS
$env:PATH += ";C:\path\to\slang\bin"  # Windows PowerShell
```

### Ошибка загрузки библиотеки

```bash
# Linux: проверьте кэш библиотек
ldconfig -p | grep slang

# Обновите кэш
sudo ldconfig

# Или установите LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Конфликт версий

Если установлены несколько версий (Vulkan SDK + локальная сборка):

```bash
# Проверьте, какая версия используется
slangc --version

# Явно укажите путь в CMake
set(Slang_DIR "/path/to/slang/lib/cmake/slang")
find_package(Slang REQUIRED)