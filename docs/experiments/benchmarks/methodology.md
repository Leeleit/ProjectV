# Benchmark methodology

Стандарт измерений для прототипов в `experiments/<slug>/prototype/`. Если эксперимент не включает измерений — этот файл
необязателен.

---

## 1. Цель

Получить **воспроизводимые** цифры, которые mainline-агент может забрать и на основании которых принять решение об
интеграции. «Mean + одна цифра» — недостаточно.

---

## 2. Окружение

- **Язык прототипа:** C++ (если тесно связан с hot-path ProjectV), либо Python (если разведочный анализ/визуализация).
- **Компилятор:** Clang (текущая mainline-версия — Clang 22.1.x). Флаги по умолчанию:
  `-O3 -march=native -DNDEBUG -std=c++26`.
- **CPU:** фиксировать модель, governor (`performance`), pinning (если возможно — один core для однопоточных,
  фиксированный набор для многопоточных).
- **GPU (если релевантно):** фиксировать модель, драйвер, vendor. Vulkan-расширения — фиксировать включённые.
- **Тепловой режим:** прогрев не менее 3 секунд перед замерами.

---

## 3. Протокол замера

1. **Warm-up:** не менее 10 итераций (или 3 секунд, что больше). Результат не учитывается.
2. **Замеры:** N = 1000 (по умолчанию). Каждая итерация — отдельный запуск таймера с холодным кэшем, если данные больше
   L2/L3 (зависит от прототипа).
3. **Метрики:**
    - mean
    - median
    - p95
    - p99
    - std
    - min / max (опц.)
4. **Формат вывода:**
    - machine-readable: `results.csv` или `results.json` (одна строка на конфигурацию).
    - human-readable: `RESULTS.md` — сводная таблица + графики (если есть данные).

---

## 4. Изоляция от шума

- Замеры на isolated CPU (если возможно; иначе — фиксировать, что shared).
- Проверить отсутствие фоновых процессов (`htop`, `ps aux`); зафиксировать в `RESULTS.md`.
- Один прогон = одна конфигурация; между конфигурациями — перезапуск процесса.
- Повторять весь эксперимент 3 раза в разное время суток (если требуется высокая точность).

---

## 5. Привязка к ProjectV

Даже если прототип standalone, в `README.md` эксперимента обязательна секция **«Mapping to ProjectV hot-path»**:

- Какой именно участок движка соответствует прототипу.
- Какие допущения/упрощения относительно реального hot-path.
- Что останется неизмеренным (например, GPU-side driver overhead, kernel launch latency).

---

## 6. Чего НЕ делать

- Измерять один прогон и объявлять результат.
- Сравнивать прототипы на разном железе без пометки.
- Подавлять оптимизации компилятора ради «честности».
- Использовать `std::chrono::high_resolution_clock` без проверки, что ядро не прыгает между ядрами.

---

## 7. Шаблон harness (минимальный)

```cpp
// Скелет — НЕ копировать без адаптации к эксперименту.
// Минимум: warm-up + N итераций + mean/median/p95/std.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
};

Stats Compute(const std::vector<double>& samples) {
    Stats s{};
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}
```

---

## 8. Self-check перед публикацией результатов

- [ ] Версия компилятора / драйвера / ОС зафиксированы.
- [ ] Команда сборки и запуска указана в `README.md` эксперимента.
- [ ] `results.csv` приложен.
- [ ] `RESULTS.md` содержит таблицу и интерпретацию.
- [ ] Указано, что именно мапится на ProjectV и какие допущения.