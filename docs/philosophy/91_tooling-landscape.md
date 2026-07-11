# Ландшафт инструментов

Документ описывает стек инструментов для разработки
высокопроизводительных движков. Если нужно добавить зависимость —
сначала проверь здесь.

> Принцип: где можно — стандарт; где стандарт не справляется — конкретная
> библиотека с конкретной ответственностью; всё через git submodule для
> reproducibility.

---

## Компилятор и стандартная библиотека

| Инструмент | Версия | Зачем |
|:-----------|:-------|:------|
| **Clang** | 22+ (primary dev-host) | C++26 features first, ThinLTO, PGO. |
| **GCC** | 14+ (secondary, Linux CI) | Совместимость. |
| **MSVC** | 19.4x (Visual Studio 2022 17.10+) | Windows secondary target. |
| **libc++** | latest, синхронно с Clang | `import std;` поддержка. |
| **libstdc++** | 14+ (fallback) | Стандартная поставка GCC. |

**CMake:** 3.30+ minimum. 4.0+ рекомендуется для полной поддержки
C++26 модулей. 4.4 в разработке.

См. [06_why-cpp26.md](06_why-cpp26.md), [14_compiler.md](14_compiler.md),
[15_compile-time.md](15_compile-time.md).

---

## Графика

### Vulkan SDK и loader

| Компонент | Версия | Зачем |
|:----------|:-------|:------|
| **Vulkan Headers** | 1.4.350+ | Спека и man-страницы. |
| **volk** | 1.4.350 | Meta-loader для Vulkan. Динамическая загрузка. |
| **VulkanMemoryAllocator (VMA)** | 2.1.0 | Sub-allocation, defragmentation, budget tracking. |

См. [31_vulkan.md](31_vulkan.md).

### Ассеты и шейдеры

| Компонент | Версия | Зачем |
|:----------|:-------|:------|
| **glslang / glslc** | latest | GLSL → SPIR-V компиляция. |
| **SPIRV-Cross / SPIRV-Reflect** | latest | Шейдерная рефлексия. |
| **fastgltf** | v0.1.0 (pre-1.0) | Быстрый glTF 2.0 парсер. |
| **draco** | 1.3.0 | Google Draco mesh compression. |
| **meshoptimizer** | v1.1.1 | Mesh simplification, vertex cache optimization, index codec. |

### Математика

| Компонент | Версия | Зачем |
|:----------|:-------|:------|
| **GLM** | 1.1.0 | Векторы, матрицы, кватернионы. |
| **`std::linalg` (P1673)** | C++26 | Стандартный BLAS. |

---

## Подсистемы движка

### ECS

| Компонент | Версия | Зачем |
|:----------|:-------|:------|
| **flecs** | v4.1 | Архетипный ECS, C99 API, lockless scheduler, hierarchies & prefabs, staging для многопоточности. |

См. [22_ecs.md](22_ecs.md).

### Физика

| Компонент | Версия | Зачем |
|:----------|:-------|:------|
| **JoltPhysics** | v5.5.0 | Rigid body physics, continuous collision detection, multi-threading. |

### UI / отладка

| Компонент | Версия | Зачем |
|:----------|:-------|:------|
| **Dear ImGui** | v1.62 | Immediate-mode UI. |
| **Tracy** | v0.13.1 | Frame profiler (CPU + GPU + memory + locks). |

Tracy 0.13 (декабрь 2025) добавил:

- Vulkan contexts через `VK_EXT_host_query_reset`.
- Динамическая загрузка Vulkan symbols.
- D3D11/D3D12 переписаны с нуля.
- AMD ROCm / rocprof поддержка.

См. [19_debugging.md](19_debugging.md).

### Аудио

| Компонент | Версия | Зачем |
|:----------|:-------|:------|
| **miniaudio** | 0.11.22 | Single-header audio playback и capture. |

### Прочее

| Компонент | Версия | Зачем |
|:----------|:-------|:------|
| **SDL** | release-3.4 | Окно, ввод, event loop, audio routing. |
| **nlohmann_json** | v3.12.0 | Scene config, asset metadata. |
| **fmt** | 12.2.0 | Type-safe форматирование. |
| **benchmark (Google)** | v1.9.4 | Микро-бенчмарки. |

---

## Профилирование (аппаратные счётчики)

### Tracy 0.13.1

Frame profiler с поддержкой CPU, GPU (Vulkan contexts через
`VK_EXT_host_query_reset`), memory, locks. Low overhead: < 1% в Release.

### AMD uProf 5.3 (май 2026)

Аппаратные счётчики на AMD Zen-архитектуре. Hardware event-based
sampling, microarchitecture analysis, cache analysis, GPU profiling
(для Instinct MI Series).

Поддержка: Linux, Windows, FreeBSD.

### Intel VTune Profiler 2026.1

Аппаратные счётчики на Intel. Hotspots, microarchitecture analysis,
XPU offload analysis, GPU analysis (для Intel Arc, Data Center GPU).

2026.1: поддержка Clearwater Forest, улучшенный XPU offload analysis.

Поддержка: Linux, Windows.

### Nsight Compute (NVIDIA)

GPU профилирование: register usage, memory throughput, divergence
analysis, occupancy.

Поддержка: только NVIDIA GPU.

---

## Сборка и анализ

### Сборка

| Инструмент | Версия | Зачем |
|:-----------|:-------|:------|
| **CMake** | 3.30+ (4.0+ рекомендуется) | Build system. |
| **Ninja** | 1.11+ | Быстрый генератор. |
| **sccache** | latest | Распределённый compile cache. |
| **LTO / ThinLTO** | Clang feature | Кросс-TU оптимизация. |
| **Ccache** | latest (alternative) | Локальный compile cache. |

### Анализ

| Инструмент | Зачем |
|:-----------|:------|
| **clang-tidy** | Статический анализ (в CI). |
| **clang-format** | Форматирование. |
| **include-what-you-use** | Избыточные `#include`. |
| **Sanitizers (ASan, TSan, UBSan, MSan)** | Runtime баги (Debug). |
| **Tracy** | Runtime профилирование. |
| **AMD uProf / Intel VTune** | Аппаратные счётчики. |
| **Nsight Compute** | GPU профилирование. |
| **RenderDoc** | Frame capture, draw call inspection. |
| **Vulkan Validation Layers** | API misuse в Debug. |

---

## Submodule policy

Все внешние зависимости — git submodule'и. Никаких FetchContent,
auto-download.

Принципы:

- **`include-what-you-pay-for`** — каждая зависимость оправдана.
- **Предпочитать header-only** — упрощает интеграцию.
- **MIT / BSD / Apache 2.0 preferred** — избегаем GPL.
- **Версия фиксируется в submodule SHA**, не в теге — reproducible
  builds.

---

## Как добавить новую зависимость

1. Проверь таблицу выше.
2. Если нет — открой issue с обоснованием: какую проблему решает,
   альтернативы, лицензия, размер бинарника, compile time impact.
3. Pin в submodule с конкретным commit SHA.
4. Добавь в build system.
5. Обнови этот файл.
6. Обнови engineering contracts, если зависимость меняет контракт.

---

## Пример: применение в движке

В ProjectV используется стек из таблицы выше: Clang 22 + libc++ + CMake
4.x, Vulkan 1.4 + VMA 2.1 + volk, flecs v4.1 ECS, JoltPhysics v5.5,
Tracy 0.13.1 профилировщик, AMD uProf 5.3 + Intel VTune 2026.1 для
аппаратных счётчиков.

---

## Источники

Версии инструментов актуальны на июнь 2026.

- Clang: <https://clang.llvm.org/>
- CMake: <https://cmake.org/cmake/help/latest/>
- Vulkan: <https://registry.khronos.org/vulkan/>
- flecs: <https://www.flecs.dev/flecs/>
- JoltPhysics: <https://github.com/jrouwe/JoltPhysics>
- Tracy 0.13.1: <https://github.com/wolfpld/tracy/releases/tag/v0.13.1>
- VMA: <https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator>
- volk: <https://github.com/zeux/volk>
- GLM: <https://github.com/g-truc/glm>
- fmt: <https://github.com/fmtlib/fmt>
- Dear ImGui: <https://github.com/ocornut/imgui>
- AMD uProf 5.3: <https://www.amd.com/en/developer/uprof.html>
- Intel VTune 2026.1:
  <https://www.intel.com/content/www/us/en/developer/articles/release-notes/vtune-profiler/2026.html>