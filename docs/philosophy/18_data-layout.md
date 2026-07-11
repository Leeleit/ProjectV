# Расположение данных

Документ описывает padding, alignment, hot/cold splitting, SoA vs AoS.

---

## Почему компилятор добавляет padding

Компилятор выравнивает поля структуры по их естественному выравниванию.
`int` (4 байта) выравнивается по адресу, кратному 4. `double` (8 байт) —
по 8. Если предыдущее поле не даёт такой адрес, компилятор вставляет
пустые байты (padding).

Пример:

```cpp
struct Bad {
    char a;     // 1 байт
    // 3 байта padding
    int b;      // 4 байта, выровнено по 4
    char c;     // 1 байт
    // 7 байт padding
    double d;   // 8 байт
};
// sizeof(Bad) = 24, но реальных данных = 14 байт
```

17% памяти — padding. Для одного объекта незаметно. Для миллионов —
заметно.

---

## Правила оптимизации структур

### Правило 1: упорядочить поля по убыванию размера

```cpp
struct Good {
    double d;   // 8 байт
    int b;      // 4 байта
    char a;     // 1 байт
    char c;     // 1 байт
};
// sizeof(Good) = 16
```

8 байт padding внутри структуры убраны. Выигрыш: 24 → 16 = 33% экономии
памяти. Без изменения логики.

### Правило 2: явно указать alignment через `alignas`

```cpp
struct alignas(64) CacheAligned {
    std::atomic<uint32_t> counter;
    uint32_t padding[15];
};
```

Используется для false sharing protection.

### Правило 3: `std::hardware_destructive_interference_size` для portability

Константа в `<new>` (C++17), автоматически выбирается под платформу:
64 на x86, 128 на Apple Silicon.

```cpp
struct alignas(std::hardware_destructive_interference_size) ThreadLocalData {
    int counter_a;
    int counter_b;
};
```

### Правило 4: `static_assert` на размер и alignment

```cpp
struct alignas(64) Voxel {
    uint32_t material_id;
    uint32_t metadata;
    static_assert(sizeof(Voxel) == 8);
    static_assert(alignof(Voxel) == 4);
};
```

Если кто-то добавит поле и нарушит layout, компиляция упадёт.

---

## Hot/Cold splitting

Данные делятся на часто и редко используемые. Размещаются раздельно.

### Зачем

В структуре объекта часто есть «горячие» поля (используются каждый кадр)
и «холодные» (используются раз в минуту). Холодные поля занимают
кэш-линии, которые вытесняют горячие данные.

### Решение

```cpp
struct HotData {
    vec3 position;
    vec3 velocity;
    mat4 transform;
};

struct ColdData {
    std::string name;
    int64_t creation_time;
    UserData owner;
};

struct Object {
    HotData* hot;
    ColdData* cold;
};
```

Hot data обрабатывается в hot loop без захолачивания кэша.

---

## SoA (Structure of Arrays) vs AoS (Array of Structures)

### AoS (классический)

```cpp
struct Particle {
    vec3 position;
    vec3 velocity;
    float mass;
};
std::vector<Particle> particles;
```

При обработке позиций все 100 000 частиц читаются в кэш. Если нужны
только позиции, остальные поля занимают 2/3 кэш-линии впустую.

### SoA (DOD-friendly)

```cpp
struct Particles {
    std::vector<vec3> positions;
    std::vector<vec3> velocities;
    std::vector<float> masses;
};
```

При обработке позиций в кэш попадают только позиции.

### Когда что

- **SoA:** большие структуры (> 32 байт), hot loop обрабатывает часть
  полей, миллионы объектов.
- **AoS:** маленькие структуры (< 16 байт), объекты обрабатываются
  целиком (рендеринг), редкие операции.

---

## Практические инструменты

### Compiler Explorer (Godbolt)

<https://godbolt.org/> — проверка размера и alignment структуры.

### `-Wpadded` (Clang/GCC)

Предупреждает о добавленном padding в структурах. Включить в CI:

```
-Wpadded
```

Каждое предупреждение — кандидат на оптимизацию layout.

### Tracy / Perf

Профилировщик показывает cache miss-ы и связанные с ними замедления.

---

## Пример: применение в движке

В ProjectV компоненты ECS — POD-структуры с проверенным layout через
`static_assert`. Hot/cold splitting применён в основных entity-структурах:
Position/Velocity в HotData, метаданные в ColdData. SoA обеспечивается
автоматически через flecs archetypes.

---

## Источники и дальнейшее чтение

- [10_manifesto.md](10_manifesto.md) — данные важнее кода.
- [21_dod.md](21_dod.md) — Data-Oriented Design.
- [22_ecs.md](22_ecs.md) — flecs archetypes и SoA layout.
- [16_memory.md](16_memory.md) — cache coherence, MESI, false sharing.
- [05_hardware-tour.md](05_hardware-tour.md) — cache lines, alignment.