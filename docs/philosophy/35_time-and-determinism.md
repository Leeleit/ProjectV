# Время и детерминизм

Документ описывает управление временем в движке.

---

## Проблема: Variable Time Step

Классический подход: `pos += vel * dt` где `dt` — время прошлого кадра.

При 60 FPS: `dt = 0.016` с, плавное движение.
При 30 FPS: `dt = 0.033` с, шаги в 2× больше.
При 10 FPS: `dt = 0.1` с, шаги огромные, коллизии пропускаются.

Дополнительные проблемы:

- Физика не детерминирована: на разном FPS результат разный.
- На мощном ПК (144 FPS) `dt = 0.007` с, на слабом (30 FPS) `dt = 0.033` с.
- Взрыв 1000 частиц → FPS падает → `dt` растёт → частицы летят дальше →
  больше нагрузки → снежный ком.

Variable Time Step — антипаттерн для real-time движка.

---

## Решение: Fixed Time Step

Время разделено на два потока:

- **Время симуляции (фиксированное):** `dt = 1/60 с`. Физика, AI,
  игровая логика.
- **Время рендеринга (переменное):** `dt = реальное время кадра`. Камера,
  интерполяция.

```cpp
constexpr float FIXED_DT = 1.0f / 60.0f;

class GameLoop {
    float accumulator = 0.0f;

    void update(float frame_time) {
        accumulator += frame_time;

        while (accumulator >= FIXED_DT) {
            update_simulation(FIXED_DT);
            accumulator -= FIXED_DT;
        }

        float alpha = accumulator / FIXED_DT;
        render(alpha);
    }
};
```

Симуляция всегда работает на `FIXED_DT`. Рендер получает `alpha` (0..1)
для интерполяции между предыдущим и текущим состоянием.

---

## Интерполяция для плавного рендеринга

Без интерполяции: при 30 FPS рендер видит 30 состояний в секунду.
Между ними — резкие скачки.

С интерполяцией: рендер интерполирует между `previous_state` и
`current_state` по `alpha`.

```cpp
struct TransformSnapshot {
    vec3 position;
    double timestamp;
};

void on_simulation_step() {
    previous_snapshot = current_snapshot;
    current_snapshot.position = entity.position;
}

void render(float alpha) {
    vec3 render_position = lerp(
        previous_snapshot.position,
        current_snapshot.position,
        alpha
    );
}
```

Дополнительная память: 2× snapshots на entity. Минимальная цена за
плавность.

---

## Детерминизм: одинаковый результат на любом железе

Детерминизм критичен для:

- Сетевой синхронизации (lockstep multiplayer).
- Записи и воспроизведения (replay).
- Кросс-платформенной совместимости.

### Источники недетерминизма

1. **Variable time step:** при разном FPS — разная последовательность
   обновлений.
2. **Floating point:** разный порядок операций = разные ошибки
   округления.
3. **Порядок итерации:** если порядок не определён — результат может
   отличаться.
4. **Random:** недетерминирован по построению.

### Решения

1. **Fixed time step:** устраняет первый источник.
2. **Fixed-point арифметика:** устраняет второй источник.
3. **Явный порядок:** компоненты отсортированы по ID, итерация в
   порядке ID.
4. **Seeded random:** генератор с известным seed'ом.

### FP-детерминизм на x86: ловушка компилятора

`-ffast-math` (или `-Ofast`) переупорядочивает FP операции для
скорости. Это меняет округление и ломает детерминизм.

`-ffast-math` в движке **запрещён**. Используется `-O3` с явным
`-fno-finite-math-only`.

---

## Обработка «спирали смерти»

Спираль смерти: кадр длится 100 ms (нормально 16 ms). Accumulator
накапливает 100 ms = 6 шагов по 16 ms. Следующий кадр тоже 100 ms =
ещё 6 шагов. Цикл.

### Решение: clamp accumulator

```cpp
constexpr float MAX_ACCUMULATOR = 0.25f;

void update(float frame_time) {
    accumulator += frame_time;
    if (accumulator > MAX_ACCUMULATOR) {
        accumulator = MAX_ACCUMULATOR;
    }
    while (accumulator >= FIXED_DT) {
        update_simulation(FIXED_DT);
        accumulator -= FIXED_DT;
    }
}
```

При долгом frame time симуляция делает максимум `MAX_ACCUMULATOR / FIXED_DT`
шагов. Игра «замедляется», но не падает.

---

## Разные частоты для разных систем

Не все системы работают на одной частоте:

- **Физика:** 60 Hz (фиксированно).
- **AI:** 10-30 Hz (decisions не требуют обновления каждый кадр).
- **Анимация:** variable (зависит от FPS).
- **Рендер:** variable.
- **Сеть:** 10-30 Hz.

```cpp
world.system<Position, const Velocity>("Physics")
    .kind(flecs::OnUpdate)
    .interval(1.0f / 60.0f);

world.system<AIState>("AI")
    .kind(flecs::OnUpdate)
    .interval(1.0f / 20.0f);
```

flecs автоматически планирует системы с учётом interval.

---

## Time Scale: замедление и ускорение времени

Эффекты slow-motion или fast-forward требуют масштабирования `FIXED_DT`.

```cpp
float time_scale = 1.0f;

void update(float frame_time) {
    accumulator += frame_time * time_scale;
}
```

При `time_scale = 0.5` симуляция работает в 2× медленнее.

---

## Измерение и дебаг времени

### Tracy frame markers

`FrameMark` в начале/конце кадра показывает общую длительность и
декомпозицию по системам.

```cpp
FrameMarkStart("Frame");
simulation();
render();
FrameMarkEnd();
```

Tracy показывает, какая система съела больше всего времени, где
была пауза.

### Time stats

Среднее, p99, max frame time логируются для мониторинга:

```cpp
struct TimeStats {
    double avg;
    double p99;
    double max;
};
```

Целевые метрики:

- Средний кадр: < 16.6 ms (60 FPS).
- p99: < 20 ms.
- Max: < 33 ms (30 FPS minimum).

---

## Правила

1. **Fixed time step для симуляции.** `dt = 1/60 с` всегда.
2. **Variable time step для рендера.** Интерполяция по `alpha`.
3. **Детерминизм по построению.** Fixed-point, seeded random, явный
   порядок.
4. **Clamp accumulator.** Защита от спирали смерти.
5. **Разные частоты для разных систем.** Не всё на 60 Hz.
6. **Tracy для измерения.** Не гадать о времени, измерять.

---

## Пример: применение в движке

В ProjectV физика и игровая логика — на Fixed Time Step 60 Hz. Рендер —
на variable step с интерполяцией между `previous` и `current` state.
`-ffast-math` запрещён в `CMakeLists.txt`. Clamp accumulator = 0.25 с.

---

## Источники и дальнейшее чтение

- **Glenn Fiedler — Fix Your Timestep!** (gafferongames.com, 2004).
  Каноническая статья о fixed timestep + accumulator + interpolation.
  <https://gafferongames.com/post/fix_your_timestep/>
- [10_manifesto.md](10_manifesto.md) — фундаментальные принципы.
- [22_ecs.md](22_ecs.md) — ECS и фиксированный шаг.
- [23_concurrency.md](23_concurrency.md) — Job System и параллелизм.
- [34_math-and-space.md](34_math-and-space.md) — fixed-point для
  детерминизма.
- [19_debugging.md](19_debugging.md) — Tracy workflow.