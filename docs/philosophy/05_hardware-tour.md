# Аппаратный ликбез

Документ описывает базовые характеристики CPU, GPU и памяти, важные
для разработки высокопроизводительных движков.

---

## CPU — «менеджер»

CPU — **1-64 сильных ядра**, оптимизированных для последовательной
работы с непредсказуемой логикой. Каждое ядро умеет:

- **Branch prediction:** угадывать, какая ветка `if` сработает.
- **Out-of-order execution:** переставлять инструкции.
- **Speculative execution:** выполнять инструкции заранее.
- **Prefetching:** угадывать, какие данные понадобятся.

### Иерархия памяти CPU

| Уровень | Размер | Латентность | Метафора |
|:--------|:-------|:------------|:---------|
| Register | 16-32 шт × 8 байт | 0 тактов | ручка в руке |
| L1 Cache | 32-80 KB на ядро | 4-5 тактов (~1-2 нс) | рабочий стол |
| L2 Cache | 512 KB - 4 MB на ядро | 12-15 тактов (~5 нс) | ящик стола |
| L3 Cache (если есть) | 16-96 MB shared | 40-50 тактов (~15-20 нс) | шкаф в комнате |
| RAM (DDR5) | 32-512 GB | 200-300 тактов (~80-120 нс) | библиотека в другом городе |
| NVMe SSD | 1-8 TB | 50-100 µs | почта из другого города |
| SATA SSD / HDD | 1-10 TB | 200 µs - 10 ms | голубиная почта |

Главное правило: пока процессор ждёт данные из памяти, он простаивает.
200 тактов впустую = можно было выполнить 200 инструкций.

### Конкретные числа (2025-2026, x86 P-core Zen 4 / Lion Cove)

- **Cores:** 8-64 в десктоп-зоне; 64-192 в серверах.
- **Boost clock:** 3.5-6.0 GHz.
- **L1/L2/L3:** 32-128 KB / 512 KB - 4 MB / 16-256 MB.
- **Memory bandwidth:** 50-300 GB/s.
- **L1 latency:** 1-2 нс. Main memory: 80-120 нс.

---

## Apple Silicon — другая модель

ARM-процессоры Apple (M1-M4) отличаются от x86 в нескольких важных
местах:

- **Cache line: 128 байт** (vs 64 на x86). Вдвое больше данных за один
  «захват».
- **Нет L3.** Единый большой L2 до 16 MB на кластер.
- **Unified memory.** CPU и GPU разделяют одну RAM. Нет PCIe-копирования
  между VRAM и RAM.
- **Memory bandwidth:** до 800 GB/s на M3 Ultra.

Практические следствия для разработки:

- `alignas(64)` для false-sharing protection заменить на
  `alignas(std::hardware_destructive_interference_size)` для
  cross-platform совместимости.
- Apple Silicon не нуждается в async upload через PCIe — данные можно
  шарить напрямую.

---

## GPU — «фабричный цех»

GPU — **1024-16384 слабых ядер**, сгруппированных в **warps** (NVIDIA,
32 потока) или **wavefronts** (AMD, 64 потока). Все потоки в warp
выполняют **одну и ту же инструкцию** на разных данных (SIMT — Single
Instruction, Multiple Threads).

### Характеристики GPU (2025-2026, RTX 50 series)

- **Cores:** 1024-16384.
- **Throughput:** 20-120 TFLOPS FP32.
- **VRAM:** 8-96 GB (16-48 GB в prosumer-зоне).
- **Bandwidth:** 500-3000 GB/s на GDDR6X / HBM (в 5-15× больше CPU).
- **Kernel launch overhead:** 5-50 µs. Мелкие задачи в 10-100× медленнее.
- **PCIe Gen4 x16:** ~32 GB/s. В 15-100× медленнее VRAM bandwidth.

### Когда выигрывает GPU, когда — CPU

| Workload | CPU лучше | GPU лучше |
|:---------|:----------|:----------|
| Branches, unpredictable logic | да (10-20×) | нет |
| Sequential / scalar | да (10-100×) | нет |
| Massive parallelism (100k+ identical ops) | нет | да (5-50×) |
| Memory bandwidth-bound (>500 GB/s) | нет | да (5-15×) |
| Latency-sensitive (<1 ms response) | да | нет |
| Batch processing (≥1024 items per kernel) | нет | да |

**Эмпирика 2026:** «Если цикл >100k итераций и <5 ветвлений на 100 операций
— пробуй GPU. Если функция должна ответить за <1 ms — оставайся на CPU».

### Специализированные блоки GPU

Современный GPU — это не «много ядер», а **гетерогенный процессор**:

- **CUDA cores / Stream processors** — универсальные ALU.
- **Tensor cores** — матричные умножения 4×4, 8×8, 16×16 за такт.
- **RT cores** (NVIDIA, Turing+): BVH traversal и ray-triangle
  intersection в железе. ~10× быстрее software traversal.
- **Mesh shaders** — замена vertex+geometry pipeline.
- **Texture units** — sampling и фильтрация текстур.

См. [31_vulkan.md](31_vulkan.md).

---

## Латентность vs Throughput

**Латентность** — время одной операции. **Throughput** — количество
операций в секунду.

- CPU оптимизирован под **латентность**: каждая инструкция выполняется
  быстро.
- GPU оптимизирован под **throughput**: тысячи операций параллельно.

В играх важны оба, но **латентность** обычно важнее throughput:
стабильные 16.6 ms лучше, чем в среднем 12 ms со спайками до 50.

См. [10_manifesto.md](10_manifesto.md), [30_optimization.md](30_optimization.md).

---

## Диск и стриминг

NVMe SSD даёт 5-7 GB/s sequential read. Этого хватает для стриминга
больших ассетов (текстуры, воксельные данные) в реальном времени. **Но
латентность доступа к произвольному блоку** — десятки микросекунд.

См. [24_data-flow.md](24_data-flow.md).

---

## Сетевое взаимодействие CPU↔GPU

Данные CPU↔GPU идут через **PCIe** или **unified memory** (Apple
Silicon). Это **узкое место**:

- PCIe Gen4 x16: ~32 GB/s в каждую сторону.
- Это в 15-100× медленнее, чем пропускная способность VRAM.

Правило: **минимизировать CPU↔GPU round-trip per-frame**. Batch все
updates в один SSBO upload. См. [31_vulkan.md](31_vulkan.md).

---

## Пример: реализация в ProjectV

ProjectV работает на Linux + x86 (RTX 3060 Ti) как primary dev-host.
Target hardware — NVIDIA RTX 20/30/40/50 series (Turing RT cores или
новее). Hard-fail на non-RTX GPU (см. `agent/knowledge.md` §2).

---

## Источники и дальнейшее чтение

- **TheLinuxCode — CPU vs GPU in 2026** (январь 2026). Конкретные числа
  для hardware-tour.
  <https://thelinuxcode.com/cpu-vs-gpu-in-2026-a-practical-numbers-first-guide-for-developers/>
- **Acomquest — CPU vs GPU: Understanding the Architecture Difference**
  (Medium, февраль 2026).
  <https://medium.com/@indiai/cpu-vs-gpu-understanding-the-architecture-difference-26d865fc0665>
- **Travis Downs — Performance resources** blog.
  <https://travisdowns.github.io/>
- **Daniel Lemire — blog**. SIMD, fast integer parsing.
  <https://lemire.me/blog/>
- [10_manifesto.md](10_manifesto.md) — фундаментальные принципы.
- [16_memory.md](16_memory.md) — аллокаторы.
- [31_vulkan.md](31_vulkan.md) — Vulkan.