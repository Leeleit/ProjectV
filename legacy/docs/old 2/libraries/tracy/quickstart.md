# Быстрый старт Tracy

**🟢 Уровень 1: Начинающий**

## На этой странице

- [CMake интеграция](#cmake-интеграция)
- [Базовое использование](#базовое-использование)
- [Запуск сервера](#запуск-сервера)
- [Просмотр результатов](#просмотр-результатов)
- [Практические примеры](#практические-примеры)
- [Следующие шаги](#следующие-шаги)
- [См. также](#см-также)

---

## CMake интеграция

### Минимальная интеграция

```cmake
# Добавьте Tracy в ваш проект
add_subdirectory(external/tracy)
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
```

### Условное включение

```cmake
# Опция для включения/выключения Tracy
option(TRACY_ENABLE "Enable Tracy profiling" ON)

if(TRACY_ENABLE)
    add_subdirectory(external/tracy)
    target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)
    target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
endif()
```

### Настройка для разных конфигураций

```cmake
# Debug - полное профилирование
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND TRACY_ENABLE)
    target_compile_definitions(YourApp PRIVATE
        TRACY_ENABLE
        TRACY_CALLSTACK=8      # Запись стека вызовов
        TRACY_MEMORY           # Профилирование памяти
    )
endif()

# Release - минимальный overhead
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND TRACY_ENABLE)
    target_compile_definitions(YourApp PRIVATE
        TRACY_ENABLE
        TRACY_ONLY_FRAME       # Только отметки кадров
        TRACY_LOW_OVERHEAD     # Минимальный overhead
    )
endif()
```

---

## Базовое использование

### Подключение заголовочного файла

```cpp
// Основной заголовочный файл
#include "tracy/Tracy.hpp"
```

### Профилирование функции (Zone)

```cpp
void expensiveCalculation() {
    ZoneScoped;  // Автоматическое имя из имени функции
    // Ваш код...
}

void processData(const std::vector<int>& data) {
    ZoneScopedN("ProcessData");  // Явное имя зоны

    for (const auto& value : data) {
        // Вложенные зоны
        {
            ZoneScopedN("ProcessElement");
            processElement(value);
        }
    }
}
```

### Отметка кадров (Frame)

```cpp
int main() {
    // Инициализация...

    while (running) {
        FrameMark;  // Отметка начала нового кадра

        // Игровая логика
        {
            ZoneScopedN("Update");
            updateGame();
        }

        // Рендеринг
        {
            ZoneScopedN("Render");
            renderFrame();
        }

        // Презентация
        {
            ZoneScopedN("Present");
            presentFrame();
        }
    }

    return 0;
}
```

### Именованные кадры

```cpp
void renderToTexture() {
    FrameMarkNamed("RenderToTexture");
    // Рендеринг в текстуру...
}

void mainRender() {
    FrameMarkNamed("MainRender");
    // Основной рендеринг...
}
```

---

## Запуск сервера

### Получение сервера Tracy

1. **Сборка из исходников:**

```bash
cd external/tracy
mkdir build && cd build
cmake ..
make -j$(nproc)  # или cmake --build . --config Release
```

2. **Или загрузите бинарники:** [Релизы Tracy](https://github.com/wolfpld/tracy/releases)

### Запуск сервера

**Windows:**

```bash
Tracy.exe
```

**Linux/macOS:**

```bash
./tracy-profiler
```

### Альтернативные способы запуска

```bash
# С сохранением сессии в файл
./tracy-profiler -o session.tracy

# С указанием порта
./tracy-profiler -p 12345

# Запуск только сервера (без GUI)
./tracy-capture -o session.tracy
```

---

## Просмотр результатов

### Подключение к приложению

1. **Запустите сервер Tracy**
2. **Запустите ваше приложение** с включенным Tracy
3. **Нажмите "Connect"** в интерфейсе Tracy

### Основные элементы интерфейса

| Элемент            | Описание                             |
|--------------------|--------------------------------------|
| **Timeline**       | Временная шкала с зонами и событиями |
| **Frame timeline** | График времени кадров                |
| **Statistics**     | Статистика по зонам (min/max/avg)    |
| **Plots**          | Графики числовых значений            |
| **Messages**       | Текстовые сообщения                  |
| **Memory**         | Информация об аллокациях памяти      |
| **GPU zones**      | Зоны выполнения на GPU               |

### Навигация в интерфейсе

- **Zoom:** Колесо мыши или Ctrl+колесо
- **Pan:** Перетаскивание правой кнопкой мыши
- **Выбор зоны:** Клик левой кнопкой
- **Фильтрация:** Поле поиска вверху
- **Группировка по потокам:** Разделение по горизонтали

---

## Практические примеры

### Профилирование алгоритма

```cpp
void sortLargeDataset(std::vector<int>& data) {
    ZoneScopedN("SortDataset");

    TracyPlot("DatasetSize", (int64_t)data.size());

    // Разные этапы сортировки
    {
        ZoneScopedN("Preparation");
        prepareData(data);
    }

    {
        ZoneScopedN("Sorting");
        std::sort(data.begin(), data.end());
    }

    {
        ZoneScopedN("Verification");
        verifySorted(data);
    }

    TracyMessageL("Dataset sorted successfully");
}
```

### Отслеживание метрик

```cpp
void updateGameMetrics() {
    // FPS и время кадра
    static float frameTime = 0.0f;
    static int framesPerSecond = 0;

    TracyPlot("FrameTimeMs", frameTime);
    TracyPlot("FPS", (float)framesPerSecond);

    // Использование памяти
    TracyPlot("MemoryMB", getMemoryUsage() / (1024.0f * 1024.0f));

    // Игровые метрики
    TracyPlot("Entities", (int64_t)getEntityCount());
    TracyPlot("ActiveChunks", (int64_t)getActiveChunkCount());
}
```

### Сообщения и аннотации

```cpp
void loadResource(const std::string& filename) {
    ZoneScopedN("LoadResource");

    // Аннотация зоны текстом
    ZoneText(filename.c_str(), filename.size());

    TracyMessageL(("Loading resource: " + filename).c_str());

    try {
        // Загрузка ресурса...
        loadFromFile(filename);
        TracyMessageL("Resource loaded successfully");
    } catch (const std::exception& e) {
        // Цветное сообщение об ошибке
        TracyMessageC(e.what(), strlen(e.what()), 0xFF0000);
    }
}
```

---

## Следующие шаги

### После освоения быстрого старта:

1. **Изучите продвинутые возможности:**
  - [Справочник API](../tracy/api-reference.md) — все макросы и функции
  - [Основные понятия](../tracy/concepts.md) — архитектура Tracy

2. **Настройте интеграцию под ваш проект:**
  - [Интеграция](../tracy/integration.md) — CMake, конфигурация, платформы
  - [Решение проблем](../tracy/troubleshooting.md) — диагностика ошибок

3. **Оптимизируйте использование:**
  - Управление overhead
  - Селективное профилирование
  - Remote profiling

### Для ProjectV разработчиков:

- **[Интеграция в ProjectV](../tracy/projectv-integration.md)** — специфичные паттерны для воксельного движка

---

## См. также

- [Глоссарий](../tracy/glossary.md) — термины и определения
- [Основные понятия](../tracy/concepts.md) — архитектура и типы инструментов
- [Интеграция](../tracy/integration.md) — полная настройка проекта
- [Справочник API](../tracy/api-reference.md) — все макросы и функции
- [Решение проблем](../tracy/troubleshooting.md) — диагностика и исправление ошибок

← [На главную документацию Tracy](../tracy/README.md)
