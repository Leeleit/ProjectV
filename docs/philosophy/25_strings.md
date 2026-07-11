# Строки

Документ описывает работу со строками в hot path высокопроизводительных
движков.

---

## Почему `std::string` убивает производительность

`std::string` — динамически аллоцируемая структура:

- Heap allocation при создании или расширении.
- Подсчёт ссылок в COW-реализациях (gcc).
- Аллокации для каждой операции `+=`, `append`, `replace`.
- Cache miss-ы при обращении к строке через указатель.

В hot path:

- Сериализация сетевых пакетов: 1000 строк × 1 µs = 1 ms overhead.
- Логгирование: 10 000 строк × 100 ns = 1 ms overhead.
- Имена компонентов ECS: 1000 запросов × 50 ns = 50 µs.

`std::string` оправдана в:

- UI (имена кнопок, текст в диалогах).
- Конфигурация (имена параметров в JSON).
- Сохранение/загрузка (метаданные уровней).

Не оправдана в:

- Сетевой сериализации.
- Hot loop логики.
- Per-frame allocations.

---

## Решение: StringID

StringID — 64-битный хеш строки. Вместо строки передаётся ID. По ID
можно получить оригинальную строку через lookup table.

### Реализация

```cpp
struct StringID {
    uint64_t hash;

    StringID() = default;
    constexpr StringID(std::string_view s) : hash(fnv1a(s)) {}

    bool operator==(const StringID& other) const = default;
};

namespace std {
template <> struct hash<StringID> { size_t operator()(StringID id) const { return id.hash; } };
}
```

### Хеш-функция

FNV-1a — простая и достаточно хорошая:

```cpp
constexpr uint64_t fnv1a(std::string_view s) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : s) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}
```

Альтернативы: xxHash3, CityHash, MurmurHash3. Быстрее FNV-1a, но
сложнее.

### Применение

```cpp
struct EventType {
    StringID name;
};

StringID event_type("PlayerSpawned");
buffer.write(event_type.hash);

LOG("Event: {}", get_string(event_type));
```

### Где использовать StringID

- Имена компонентов, тегов, систем в ECS.
- Имена событий.
- Имена ассетов (текстуры, меши, шейдеры).
- Имена ключей в JSON-конфигурации.
- Состояния state machine.

### Где НЕ использовать StringID

- UI текст.
- Длинные документы (XML, JSON values).
- Случайные строки из пользовательского ввода.
- Динамически генерируемые имена.

---

## `std::string_view` для read-only параметров

Если функция принимает строку только для чтения и не сохраняет — принимает
`std::string_view`.

```cpp
Result<Texture> load_texture(std::string_view path);
```

Преимущества:

- Без аллокации.
- Принимает `std::string`, `const char*`, литералы без копирования.
- Zero-cost: `string_view` — два указателя.

Ограничения:

- Нельзя сохранять view, если оригинальная строка не живёт дольше view.
- Не подходит для null-terminated C API.

---

## Интернирование строк

Lookup table для обратного преобразования StringID → `std::string`
создаётся при старте движка.

```cpp
class StringPool {
    std::unordered_map<uint64_t, std::string> pool_;

public:
    StringID intern(std::string_view s) {
        auto id = StringID(s);
        if (!pool_.contains(id.hash)) {
            pool_[id.hash] = std::string(s);
        }
        return id;
    }

    const std::string& get(uint64_t hash) const {
        return pool_.at(hash);
    }
};
```

Пул заполняется на startup. Hot path использует только `StringID` и
lookup по hash.

---

## Сравнение производительности

| Подход | Размер | Аллокация | Сравнение строк | Lookup |
|:-------|:-------|:----------|:----------------|:-------|
| `std::string` | 24-32 байт (SSO) + heap | Да | O(n) посимвольно | O(n) |
| `std::string_view` | 16 байт | Нет | O(n) посимвольно | Нет |
| `const char*` | 8 байт | Нет | strcmp | Нет |
| `StringID` | 8 байт | Нет (через пул) | O(1) | O(1) |

---

## Коллизии хешей

FNV-1a — не криптографический хеш. Коллизии возможны, но редки для
коротких строк.

- Вероятность коллизии для 1000 строк с 64-битным хешем: ~2.7 × 10⁻¹⁴.
- Для 32-битного хеша и 10 000 строк: ~1.2 × 10⁻⁵ (одна на 100 000
  случаев).

32-битные хеши — слишком слабые. 64-битные — достаточно.

Если коллизия обнаружена (два разных имени дают один ID при lookup) —
исправить хеш-функцию на этапе разработки.

---

## Оптимизации памяти

### String pool в BSS

Статический пул строк, предзаполненный при компиляции:

```cpp
constexpr auto FOO = intern_constexpr("foo.bar.baz");
```

### Lazy intern

Строки интернируются при первом использовании. Hot path использует
только ID.

---

## Интеграция с существующим кодом

### Замена `std::string` на `StringID`

1. Определить `StringID` и `StringPool`.
2. Найти все `std::string` в hot path (профилировщик покажет аллокации).
3. Заменить на `StringID`.
4. Заменить `std::string` в компонентах ECS на `StringID`.
5. Lookup через `get_string(id)` — только в логах и UI.

---

## Правила

1. `std::string` — только в UI, конфигурации, editor tools.
2. Hot path — `StringID` для имён, `std::string_view` для read-only
   параметров.
3. Сериализация — через `StringID.hash`.
4. Lookup через пул — только для логов и UI.
5. Никаких аллокаций `std::string` в hot loop.

---

## Пример: применение в движке

В ProjectV компоненты ECS, имена систем, имена событий — все через
StringID с FNV-1a hash. StringPool создаётся в startup. Lookup через
`get_string(id)` — только в debug-логах. `std::string` остаётся только в
UI и editor tools.

---

## Источники и дальнейшее чтение

- **FNV-1a hash** — Fowler–Noll–Vo, простой и эффективный.
- **xxHash** — Yann Collet, быстрее FNV-1a.
- [16_memory.md](16_memory.md) — аллокаторы.
- [22_ecs.md](22_ecs.md) — компоненты ECS.
- [24_data-flow.md](24_data-flow.md) — поток данных.