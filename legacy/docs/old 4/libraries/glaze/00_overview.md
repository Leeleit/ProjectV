# Glaze — Обзор

🟢 **Уровень 1: Базовый**

## Что такое Glaze

Glaze — это библиотека для сериализации данных и compile-time reflection в C++23. Она предоставляет
высокопроизводительную альтернативу традиционным подходам без использования макросов или RTTI.

## Ключевые характеристики

| Характеристика  | Значение                             |
|-----------------|--------------------------------------|
| Скорость JSON   | Один из самых быстрых парсеров       |
| Скорость Binary | Zero-copy где возможно               |
| Compile-time    | Полная проверка типов при компиляции |
| Зависимости     | Header-only, только STL              |
| Лицензия        | MIT                                  |

## Основные возможности

### Compile-time Reflection

```cpp
struct Player {
    std::string name;
    int health;
    float position[3];

    struct glaze {
        using T = Player;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "health", &T::health,
            "position", &T::position
        );
    };
};
```

### JSON сериализация

```cpp
Player player{.name = "Hero", .health = 100, .position = {1.0f, 2.0f, 3.0f}};

// Запись
std::string json;
glz::write_json(player, json);
// {"name":"Hero","health":100,"position":[1.0,2.0,3.0]}

// Чтение
Player loaded;
glz::read_json(loaded, json);
```

### Бинарная сериализация

```cpp
// Компактный бинарный формат
std::vector<uint8_t> buffer;
glz::write_binary(player, buffer);

// Чтение
Player loaded;
glz::read_binary(loaded, buffer);
```

## Поддерживаемые типы

- Все базовые типы (`int`, `float`, `bool`, `char`)
- `std::string`, `std::string_view`
- `std::vector`, `std::array`, `std::list`
- `std::map`, `std::unordered_map`
- `std::optional`, `std::variant`
- `std::unique_ptr`, `std::shared_ptr`
- Enum классы
- Вложенные структуры
- Массивы C-style

## Сравнение с альтернативами

| Библиотека | Макросы | Скорость JSON | Binary | C++ версия |
|------------|---------|---------------|--------|------------|
| **glaze**  | Нет     | Очень высокая | Да     | C++20+     |
| nlohmann   | Нет     | Средняя       | Нет    | C++11      |
| rapidjson  | Нет     | Высокая       | Нет    | C++11      |
| cereal     | Да      | Средняя       | Да     | C++11      |

## Применение в ProjectV

- **Сериализация ECS компонентов** — сохранение состояния мира
- **Конфигурация** — загрузка настроек из JSON
- **ImGui Inspector** — автоматическая генерация UI
- **Сетевой протокол** — бинарная сериализация для мультиплеера (будущее)

## Требования

- **C++20** минимум, **C++23** рекомендуется
- Поддержка `<span>`, `<expected>`
- Совместимые компиляторы: GCC 11+, Clang 14+, MSVC 19.30+

## Структура документации

| Файл          | Описание                              |
|---------------|---------------------------------------|
| 00_overview   | Этот файл                             |
| 01_quickstart | Быстрый старт и примеры использования |

## Полезные ссылки

- [GitHub: stephenberry/glaze](https://github.com/stephenberry/glaze)
- [Serialization Strategy](../../architecture/practice/11_serialization.md)
- [Reflection с Glaze](../../architecture/practice/13_reflection.md)
