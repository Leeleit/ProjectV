# Философия времени и детерминизма: Фиксированный шаг

В академических примерах физику двигают так: `pos += vel * dt` (где `dt` — время прошлого кадра). Но если FPS просел до
30, `dt` станет в два раза больше, пуля пролетит сквозь стену, а игрок прыгнет выше обычного. В нашем движке физика и
игровая логика **обязаны** использовать Fixed Time Step (фиксированный шаг) с паттерном "Accumulator". Рендер может
работать на 144 FPS или на 20 FPS, но физика всегда обновляется строгими тиками (например, 60 раз в секунду). Это даёт
детерминизм (игра работает одинаково на любом железе).

---

## Проблема: Variable Time Step (переменный шаг)

```cpp
// ПЛОХО: переменный шаг времени
void update_physics(float dt) {
    // dt может быть 0.016 (60 FPS), 0.033 (30 FPS), 0.1 (10 FPS)
    position += velocity * dt;
    velocity += acceleration * dt;
}
```

**Что происходит при проседании FPS:**

1. **Физика становится "прыгающей":**
  - 60 FPS: `dt = 0.016` → маленькие шаги, плавное движение
  - 30 FPS: `dt = 0.033` → шаги в 2 раза больше, объекты "телепортируются"
  - 10 FPS: `dt = 0.1` → огромные шаги, коллизии пропускаются

2. **Не детерминировано:**
  - На мощном ПК (144 FPS): `dt = 0.0069`
  - На слабом ПК (30 FPS): `dt = 0.033`
  - Физика даёт разные результаты на разном железе

3. **Чувствительность к лагам:**
  - Взрыв создаёт 1000 частиц → FPS падает → `dt` увеличивается → частицы летят дальше → больше лагов → снежный ком

> **Метафора:** Представь фильм. Фильмы показывают 24 кадра в секунду, но реальный мир не двигается рывками. Рендер —
> это просто "фотограф", который делает снимки. Логика — это реальный мир. Если фотограф устал и делает снимки реже (
> низкий FPS), это не значит, что мир замедлился. Законы физики от частоты фотографий не меняются. Variable Time Step —
> это когда ты пытаешься изменить скорость времени в зависимости от того, как часто фотограф щёлкает затвором.

---

## Решение: Fixed Time Step (фиксированный шаг)

Мы разделяем **время рендеринга** (variable) и **время симуляции** (fixed).

```cpp
constexpr float FIXED_DT = 1.0f / 60.0f; // 60 Гц физика

class GameLoop {
    float accumulator = 0.0f;
    float current_time = 0.0f;

    void update(float frame_time) {
        // 1. Накопление времени
        accumulator += frame_time;

        // 2. Фиксированные обновления физики
        while (accumulator >= FIXED_DT) {
            update_physics(FIXED_DT); // Всегда одинаковый шаг!
            current_time += FIXED_DT;
            accumulator -= FIXED_DT;
        }

        // 3. Интерполяция для плавного рендеринга
        float alpha = accumulator / FIXED_DT;
        render(alpha); // alpha ∈ [0, 1)
    }
};
```

**Как это работает:**

1. **Accumulator** накапливает реальное прошедшее время
2. **While цикл** выполняет физику фиксированными шагами, пока есть накопленное время
3. **Alpha** — коэффициент интерполяции между последним и предпоследним состоянием физики

---

## Mermaid диаграмма: Fixed Time Step vs Variable Time Step

```mermaid
gantt
    title Сравнение Fixed vs Variable Time Step
    dateFormat  HH:mm:ss.SSS
    axisFormat %S.%L s

    section Variable Time Step (Плохо)
    Кадр 1 (30 FPS) :var1, 2026-01-01 00:00:00.000, 33ms
    Физика шаг 0.033 :after var1, 33ms
    Кадр 2 (60 FPS) :var2, after var1, 16ms
    Физика шаг 0.016 :after var2, 16ms
    Кадр 3 (10 FPS) :var3, after var2, 100ms
    Физика шаг 0.1 :after var3, 100ms

    section Fixed Time Step (Хорошо)
    Кадр 1 (30 FPS) :fix1, 2026-01-01 00:00:00.000, 33ms
    Физика тик 1 :fix1_phy1, after fix1, 16ms
    Физика тик 2 :fix1_phy2, after fix1_phy1, 16ms
    Кадр 2 (60 FPS) :fix2, after fix1, 16ms
    Физика тик 3 :fix2_phy1, after fix2, 16ms
    Кадр 3 (10 FPS) :fix3, after fix2, 100ms
    Физика тик 4 :fix3_phy1, after fix3, 16ms
    Физика тик 5 :fix3_phy2, after fix3_phy1, 16ms
    Физика тик 6 :fix3_phy3, after fix3_phy2, 16ms
    Физика тик 7 :fix3_phy4, after fix3_phy3, 16ms
    Физика тик 8 :fix3_phy5, after fix3_phy4, 16ms
    Физика тик 9 :fix3_phy6, after fix3_phy5, 16ms

    section Легенда
    Кадр рендеринга :crit, active
    Шаг физики :done
```

**Объяснение диаграммы:**

- **Variable Time Step:** Размер шага физики зависит от FPS → нестабильность
- **Fixed Time Step:** Физика всегда обновляется с фиксированным интервалом (16 мс = 60 Гц)
- **При 10 FPS (100 мс кадр):** Fixed Time Step выполняет 6 шагов физики за один кадр
- **Детерминизм:** Независимо от FPS, физика делает одинаковое количество обновлений за секунду

---

## Интерполяция для плавного рендеринга

Физика обновляется с фиксированной частотой (например, 60 Гц), но рендеринг может быть чаще (144 Гц). Чтобы избежать
дёрганий:

```cpp
struct PhysicsState {
    Vec3 position;
    Quat rotation;
    Vec3 velocity;
};

class InterpolatedRenderer {
    PhysicsState previous;  // Состояние на тик T-1
    PhysicsState current;   // Состояние на тик T

    void update_physics(const PhysicsState& new_state) {
        previous = current;
        current = new_state;
    }

    void render(float alpha) {
        // Интерполяция между previous и current
        Vec3 render_pos = lerp(previous.position, current.position, alpha);
        Quat render_rot = slerp(previous.rotation, current.rotation, alpha);

        draw_object(render_pos, render_rot);
    }
};
```

**Alpha вычисляется так:**

```
alpha = accumulator / FIXED_DT
```

Если физика обновилась в момент T, а рендеринг происходит в момент T + 0.3 * FIXED_DT, то `alpha = 0.3`. Мы рендерим
объект на 30% пути между его позициями в тиках T-1 и T.

> **Для понимания:** Представь анимацию в кино. Актер двигается плавно (физика с fixed step). Камера делает снимки 24
> раза в секунду (рендеринг). Если снимок сделан между двумя позициями актёра, на фото будет небольшое размытие (
> интерполяция). Без интерполяции актёр "телепортировался" бы между кадрами. С интерполяцией движение выглядит плавным,
> даже если физика обновляется реже, чем рендеринг.

---

## Детерминизм: игра должна работать одинаково везде

Детерминизм — это когда при одинаковых вводах игра даёт одинаковые результаты, независимо от:

- FPS (30, 60, 144)
- Количества ядер CPU
- Платформы (Windows/Linux)
- Сборки (Debug/Release)

### Как достичь детерминизма:

#### 1. Fixed-point математика вместо float

```cpp
using Fixed32 = int32_t; // 16.16 формат

Fixed32 add(Fixed32 a, Fixed32 b) { return a + b; }
Fixed32 mul(Fixed32 a, Fixed32 b) { return (a * b) >> 16; }
```

Float операции не детерминированы на разных CPU (разная точность, rounding modes).

#### 2. Фиксированный порядок обновления

```cpp
// ПЛОХО: parallel_for может менять порядок
parallel_for(entities, [](auto& e) { e.update(); });

// ХОРОШО: фиксированный порядок
for (auto& e : entities) { e.update(); }
```

#### 3. Детерминированный рандом

```cpp
class DeterministicRandom {
    uint32_t seed;

public:
    explicit DeterministicRandom(uint32_t s) : seed(s) {}

    uint32_t next() {
        seed = seed * 1103515245 + 12345;
        return seed;
    }
};

// Все клиенты используют одинаковый seed
```

#### 4. Фиксированные тики сетевой синхронизации

```cpp
constexpr int NETWORK_TICK_RATE = 30; // 30 Гц
constexpr float NETWORK_DT = 1.0f / NETWORK_TICK_RATE;
```

---

## Обработка "спирали смерти" (spiral of death)

Что если физика не успевает за fixed step? Например, сложная сцена требует 20 мс на шаг физики, но fixed step = 16 мс.

```cpp
void update(float frame_time) {
    accumulator += frame_time;

    // Ограничиваем максимальное количество шагов за кадр
    constexpr int MAX_STEPS = 5;
    int steps = 0;

    while (accumulator >= FIXED_DT && steps < MAX_STEPS) {
        update_physics(FIXED_DT);
        accumulator -= FIXED_DT;
        steps++;
    }

    // Если накопилось слишком много времени — сбрасываем
    if (accumulator > FIXED_DT * 5) {
        PV_LOG_WARNING("Physics can't keep up, resetting accumulator");
        accumulator = 0.0f;
    }
}
```

**Стратегии при отставании:**

1. **Пропустить кадры рендеринга:** Лучше показать старый кадр, чем некорректную физику
2. **Увеличить fixed step временно:** С 60 Гц → 30 Гц в сложных сценах
3. **Упростить физику:** Отключить сложные коллизии для дальних объектов

---

## Разные частоты для разных систем

Не все системы нуждаются в одинаковой частоте обновления:

```cpp
constexpr float PHYSICS_DT = 1.0f / 60.0f;  // 60 Гц - физика
constexpr float AI_DT = 1.0f / 30.0f;       // 30 Гц - AI
constexpr float NETWORK_DT = 1.0f / 20.0f;  // 20 Гц - сетевая синхронизация

class MultiRateUpdate {
    float physics_acc = 0.0f;
    float ai_acc = 0.0f;
    float network_acc = 0.0f;

    void update(float frame_time) {
        physics_acc += frame_time;
        ai_acc += frame_time;
        network_acc += frame_time;

        while (physics_acc >= PHYSICS_DT) {
            update_physics(PHYSICS_DT);
            physics_acc -= PHYSICS_DT;
        }

        while (ai_acc >= AI_DT) {
            update_ai(AI_DT);
            ai_acc -= AI_DT;
        }

        while (network_acc >= NETWORK_DT) {
            sync_network(NETWORK_DT);
            network_acc -= NETWORK_DT;
        }
    }
};
```

**Почему разные частоты:**

- **Физика:** Требует высокой частоты для точности коллизий
- **AI:** Может работать реже (игрок не заметит задержку 33 мс)
- **Сеть:** Ограничена пропускной способностью, 20 Гц достаточно

---

## Time Scale: замедление и ускорение времени

Иногда нужно замедлить время (bullet time) или ускорить (fast forward):

```cpp
float g_time_scale = 1.0f; // 1.0 = нормальная скорость

void update(float frame_time) {
    float scaled_time = frame_time * g_time_scale;
    accumulator += scaled_time;

    while (accumulator >= FIXED_DT) {
        update_physics(FIXED_DT); // Физика всегда с FIXED_DT!
        accumulator -= FIXED_DT;
    }
}

// Bullet time (замедление в 10 раз)
g_time_scale = 0.1f;

// Fast forward (ускорение в 2 раза)
g_time_scale = 2.0f;
```

**Важно:** `FIXED_DT` не меняется! Меняется только `scaled_time`, который попадает в accumulator. Физика всё равно
обновляется с фиксированным шагом, просто accumulator наполняется медленнее или быстрее.

---

## Измерение и дебаг времени

### 1. Статистика

```cpp
struct TimeStats {
    float fps = 0.0f;
    float frame_time = 0.0f;
    float physics_time = 0.0f;
    float render_time = 0.0f;
    int physics_steps_per_frame = 0;
};

// Показываем в ImGui
ImGui::Text("FPS: %.1f", stats.fps);
ImGui::Text("Frame: %.2f ms", stats.frame_time * 1000.0f);
ImGui::Text("Physics: %.2f ms (%d steps)",
    stats.physics_time * 1000.0f, stats.physics_steps_per_frame);
```

### 2. Графики (Tracy)

```cpp
void update_physics(float dt) {
    ZoneScopedN("Physics Update");
    // ...
    TracyPlot("Physics Steps", static_cast<float>(steps));
    TracyPlot("Accumulator", accumulator);
}
```

### 3. Визуализация тиков

```cpp
void debug_draw_time() {
    // Рисуем тики физики на временной шкале
    for (int i = 0; i < 10; ++i) {
        float x = 10 + i * 20;
        debug_draw::line({x, 10}, {x, 30}, Color::green);
    }

    // Рисуем текущее положение рендеринга
    float alpha_x = 10 + 9 * 20 + alpha * 20;
    debug_draw::circle({alpha_x, 20}, 5, Color::red);
}
```

---

## Золотые правила

1. **Всегда Fixed Time Step для физики и логики.** Никаких `dt` в hot path.
2. **Интерполяция для плавного рендеринга.** Рендеринг между состояниями физики.
3. **Детерминизм важнее производительности.** Игра должна работать одинаково везде.
4. **Разные частоты для разных систем.** Физика 60 Гц, AI 30 Гц, сеть 20 Гц.
5. **Обрабатывай отставание.** Ограничивай максимальное количество шагов за кадр.

> **Метафора итоговая:** Представь метроном (фиксированный шаг) и музыканта (рендеринг). Метроном отбивает строгие
> доли (физика 60 Гц). Музыкант играет между долями, создавая плавную мелодию (интерполяция). Если музыкант уста
