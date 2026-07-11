# Математика и пространство

Документ описывает проблемы численной точности в больших мирах и
методы их решения.

---

## Проблема: Float Jitter

Числа с плавающей точкой (`float`, `double`) имеют ограниченную
точность. При больших значениях точность падает экспоненциально.

ULP (Unit in the Last Place) — расстояние между двумя соседними
представимыми числами:

| Значение | ULP (float) |
|:---------|:------------|
| 1.0 | 1.19 × 10⁻⁷ |
| 100.0 | 7.63 × 10⁻⁶ |
| 10 000.0 | 7.63 × 10⁻⁴ |
| 1 000 000.0 | 6.25 × 10⁻² |
| 100 000 000.0 | 8.0 |

При `x = 1 000 000` округление до 0.06 — это 6 см ошибки при
позиционировании объекта в метрах.

В voxel world: chunk (16×16×16) на расстоянии 10 000 от origin теряет
точность в десятки миллиметров. Грани chunk'ов не совпадают.

---

## Решение 1: целочисленные координаты для логики

Воксели хранятся в integer grid. Позиции в ECS — `int32_t`. Логические
операции (chunk lookup, neighbor detection) — целочисленные.

```cpp
struct VoxelPosition {
    int32_t x, y, z;
    int32_t lod;
};

int64_t squared_distance(const VoxelPosition& a, const VoxelPosition& b) {
    int64_t dx = a.x - b.x;
    int64_t dy = a.y - b.y;
    int64_t dz = a.z - b.z;
    return dx*dx + dy*dy + dz*dz;
}
```

Целочисленная арифметика без jitter. Точность — до единицы.

---

## Решение 2: Origin Shifting

Для физики, рендеринга, AI — координаты относительно origin.
Origin периодически сдвигается, чтобы объекты оставались вблизи нуля.

```cpp
struct Transform {
    vec3 local_position;
    SectorId sector;
};

// На каждом кадре:
if (player.local_position.x > SECTOR_SIZE / 2) {
    player.sector.x += 1;
    player.local_position.x -= SECTOR_SIZE;
}
```

Сектор — обычно 1024 метра (32 чанка по 16 м + запас). При секторе 1024
м, `float` точен до 0.06 мм.

### Применение

- Player position: сектор + local.
- Camera position: сектор + local.
- Physics simulation: local space.
- Networking: глобальные координаты (сектор + local).

---

## Решение 3: Fixed-Point арифметика для физики

Fixed-point — целое число с фиксированной точкой. Например, `int32_t`
с 16.16 форматом: 16 бит целая часть, 16 бит дробная.

```cpp
using Fixed32 = int32_t;

Fixed32 operator+(Fixed32 a, Fixed32 b) { return a + b; }
Fixed32 operator*(Fixed32 a, Fixed32 b) {
    return (int64_t(a) * b) >> 16;
}
```

Преимущества:

- Детерминизм: одна и та же операция даёт один и тот же результат на
  любом железе.
- Скорость: целочисленные операции быстрее floating point на старом
  железе.
- Точность: предсказуемая.

Недостатки:

- Ограниченный диапазон.
- Сложность в коде.

---

## Решение 4: двойная точность только где нужно

`double` точнее `float` в 2× (52 бита мантиссы vs 23). Используется
там, где `float` не хватает.

### Где использовать `double`

- Глобальные координаты мира (для consistency между секторами).
- Сетевая сериализация (детерминизм).
- Астрономические расчёты.

### Где НЕ использовать `double`

- Hot path рендеринга: GPU оперирует `float`, конверсия — overhead.
- Per-pixel расчёты: `float` достаточно.
- Анимация.

Правило: `float` по умолчанию, `double` только с обоснованием.

---

## Apple Silicon: особые правила

Apple Silicon GPU (M1-M4) имеет специфику:

- `half` precision поддерживается нативно (FP16).
- Tensor cores используют BF16 для ML inference.
- Vector units оптимизированы для `float` и `half`, `double` —
  эмулируется.

---

## Современные числа для cache latency (2025-2026)

| Уровень | Размер | Латентность (x86) | Латентность (Apple Silicon) |
|:--------|:-------|:------------------|:-----------------------------|
| L1 | 32-80 KB | 1-2 нс | 1-2 нс |
| L2 | 512 KB - 4 MB | 5 нс | 3-4 нс |
| L3 (если есть) | 16-96 MB | 15-20 нс | нет L3 |
| RAM (DDR5 / unified) | 32-512 GB | 80-120 нс | 80-100 нс |

Конкретные числа — порядок величин для современного железа.

---

## Правила

1. **Voxel coordinates:** целочисленные `int32_t` или `int64_t`.
2. **Entity positions:** sector + local (origin shifting).
3. **Physics:** fixed-point для детерминизма, `double` для consistency.
4. **Rendering:** `float` (GPU-native).
5. **`double` только с обоснованием.** По умолчанию — `float`.

---

## Пример: применение в движке

В ProjectV voxel coordinates — `int32_t`. Entity positions — sector + local.
Physics использует fixed-point для детерминизма сетевой синхронизации.
Rendering — `float` для всех GPU буферов.

---

## Источники и дальнейшее чтение

- **What Every Computer Scientist Should Know About Floating-Point
  Arithmetic** — David Goldberg, ACM Computing Surveys, 1991.
- **Kahan summation** — алгоритм компенсационного суммирования.
- [05_hardware-tour.md](05_hardware-tour.md) — численные характеристики
  кэша.
- [35_time-and-determinism.md](35_time-and-determinism.md) — fixed-point
  для детерминизма.