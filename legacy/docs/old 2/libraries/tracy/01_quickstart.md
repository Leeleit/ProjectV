# Быстрый старт Tracy

**🟢 Уровень 1: Начинающий** — Минимальная интеграция за 5 минут.

---

## Минимальный пример

### 1. Подключение заголовочного файла

```cpp
#include "tracy/Tracy.hpp"
```

### 2. Профилирование функции

```cpp
void myFunction() {
    ZoneScoped;  // Автоматическое имя из имени функции

    // Ваш код...
}
```

### 3. Отметка кадра

```cpp
int main() {
    while (running) {
        FrameMark;  // Отметка начала нового кадра

        // Игровая логика...
    }
    return 0;
}
```

---

## Запуск сервера

### Получение сервера

**Вариант 1: Скачать бинарники**

[Релизы Tracy](https://github.com/wolfpld/tracy/releases) — готовые бинарники для Windows, Linux, macOS.

**Вариант 2: Сборка из исходников**

```bash
cd external/tracy
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Запуск

**Windows:**

```bash
Tracy.exe
```

**Linux/macOS:**

```bash
./tracy-profiler
```

---

## Подключение к приложению

1. Запустите сервер Tracy
2. Запустите ваше приложение с встроенным Tracy
3. В интерфейсе Tracy нажмите "Connect" или введите адрес

По умолчанию Tracy слушает порт **8947**.

---

## Основные элементы интерфейса

| Элемент        | Описание                             |
|----------------|--------------------------------------|
| Timeline       | Временная шкала с зонами и событиями |
| Frame timeline | График времени кадров                |
| Statistics     | Статистика по зонам (min/max/avg)    |
| Plots          | Графики числовых значений            |
| Messages       | Текстовые сообщения                  |
| Memory         | Информация об аллокациях             |
| GPU zones      | Зоны выполнения на GPU               |

### Навигация

- **Zoom:** Колесо мыши
- **Pan:** Перетаскивание правой кнопкой
- **Выбор зоны:** Клик левой кнопкой
- **Фильтрация:** Поле поиска вверху

---

## Именованные зоны

```cpp
void processData() {
    ZoneScopedN("ProcessData");  // Явное имя зоны

    // Код...
}
```

## Цветные зоны

```cpp
void renderFrame() {
    ZoneScopedC(0xFF0000);  // Красный цвет

    // Код...
}
```

## Именованные и цветные зоны

```cpp
void updatePhysics() {
    ZoneScopedNC("Physics", 0x00FF00);  // Зелёная зона с именем

    // Код...
}
```

---

## Графики (Plots)

```cpp
void updateMetrics() {
    TracyPlot("FPS", framesPerSecond);
    TracyPlot("MemoryMB", memoryUsage / (1024.0 * 1024.0));
    TracyPlot("EntityCount", (int64_t)entityCount);
}
```

---

## Сообщения

```cpp
void loadResource(const char* filename) {
    TracyMessageL("Loading resource...");

    // Код загрузки...

    TracyMessageL("Resource loaded");
}
```

Цветные сообщения:

```cpp
TracyMessageLC("Error occurred!", 0xFF0000);  // Красное сообщение
```

---

## Полный минимальный пример

```cpp
#include "tracy/Tracy.hpp"
#include <chrono>
#include <thread>

void doWork() {
    ZoneScopedN("DoWork");

    // Имитация работы
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    TracyPlot("WorkTimeMs", 10.0);
}

int main() {
    TracyMessageL("Application started");

    for (int frame = 0; frame < 1000; frame++) {
        FrameMark;

        {
            ZoneScopedN("Update");
            doWork();
        }

        {
            ZoneScopedN("Render");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        TracyPlot("Frame", (int64_t)frame);
    }

    TracyMessageL("Application finished");
    return 0;
}
```

---

## Компиляция

### CMake (минимальный)

```cmake
add_subdirectory(external/tracy)
target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
```

### Командная строка (GCC/Clang)

```bash
g++ -DTRACY_ENABLE -I external/tracy/public main.cpp external/tracy/public/TracyClient.cpp -o app -lpthread
