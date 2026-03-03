# CMake Guide для ProjectV

Полное руководство по работе с CMake в проекте ProjectV — от основ до продвинутых оптимизаций для воксельного движка.

---

## 📚 Содержание

| # | Документ                                               | Уровень        | Описание                               |
|---|--------------------------------------------------------|----------------|----------------------------------------|
| 1 | [Основы и структура](01_basics-structure.md)           | 🟢 Начинающий  | Modern CMake, targets                  |
| 2 | [Управление зависимостями](02_dependencies.md)         | 🟡 Средний     | FetchContent, find_package, submodules |
| 3 | [Конфигурация сборки](03_build-configuration.md)       | 🟢 Начинающий  | Debug/Release, флаги компилятора       |
| 4 | [Продвинутые оптимизации](04_advanced-optimization.md) | 🔴 Продвинутый | Unity Builds, PCH, CCache              |
| 5 | [Кросс-платформенность](05_cross-platform.md)          | 🔴 Продвинутый | Windows/Linux, кросс-компиляция        |
| 6 | [Решение проблем и шаблоны](06_troubleshooting-ide.md) | 🟡 Средний     | Диагностика, CLion, шаблоны            |

---

## 🎯 Learning Path

### Для начинающих

```
01_basics-structure.md → 02_dependencies.md → 03_build-configuration.md
```

### Для опытных разработчиков

```
04_advanced-optimization.md → 05_cross-platform.md → 06_troubleshooting-ide.md
```

---

## ⚡ Быстрый старт

### Первая сборка

```bash
# Клонировать с подмодулями
git clone --recursive https://github.com/yourname/ProjectV.git
cd ProjectV

# Конфигурация
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Сборка
cmake --build build --parallel
```

### Требования

- **CMake 3.20+**
- **Компилятор C++20** (MSVC 2019+, GCC 11+, Clang 12+)
- **Vulkan SDK 1.3+**
- **Git** (для подмодулей)

---

## 📋 Зависимости ProjectV

### Активные (подмодули)

| Библиотека | Назначение    |
|------------|---------------|
| SDL3       | Окна, ввод    |
| volk       | Vulkan loader |
| VMA        | Память GPU    |

### Опциональные (закомментированы)

| Библиотека  | Назначение     |
|-------------|----------------|
| glm         | Математика     |
| flecs       | ECS            |
| JoltPhysics | Физика         |
| ImGui       | Debug UI       |
| Tracy       | Профилирование |

---

## 🔧 Частые команды

| Задача                   | Команда                                           |
|--------------------------|---------------------------------------------------|
| Конфигурация             | `cmake -B build`                                  |
| Сборка                   | `cmake --build build --parallel`                  |
| Очистка                  | `cmake --build build --target clean`              |
| Указать Vulkan SDK       | `cmake -DCMAKE_PREFIX_PATH="C:/VulkanSDK/..." ..` |
| Инициализация подмодулей | `git submodule update --init --recursive`         |

---

## 📖 Дополнительные ресурсы

- [CMake Documentation](https://cmake.org/documentation/)
- [Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
- [Vulkan SDK](https://vulkan.lunarg.com/)
