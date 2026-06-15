# DefenseBriefer_1.md — Памятка Тиммейта 1: Стек и сборка

**Участник:** [Имя Тимейта 1]
**Слот:** 2:30–4:00 (1:30 минуты)
**Что покрываю:** технологический стек, система сборки, тесты, метрики производительности
**Что НЕ покрываю:** архитектура выбора стека (le1t), voxel-мир (Тиммейт 2), тени (Тиммейт 3), физика (Тиммейт 4), демо (Тиммейт 5)

---

## 1. Шапка выступления

> «Добрый день, меня зовут **[Имя Тимейта 1]**, я расскажу про технологический стек и систему сборки проекта.»

---

## 2. Что говорить verbatim (1:30, ~220 слов)

> «Технологический стек ProjectV — это C++26, Vulkan 1.4 и Data-Oriented Design с Entity-Component System. Целевая платформа — Windows 10/11 и Linux Arch, обе сборки проходят с зелёным статусом.
>
> Компилятор — Clang 22.1.6. На Windows используется clang-cl, на Linux — нативный clang 22 с линкером lld 22 и стандартной библиотекой libc++ 16. Миграция на libc++ прошла в рамках Tier 2.5: это даёт нам полный доступ к std::simd и std::expected без оглядки на ABI-различия libstdc++.
>
> Сборка организована через CMake 4.0 с системой пресетов. У нас 7 отладочных пресетов и 8 релизных: для каждой платформы есть configure/build/tests варианты. Release-сборка использует флаги `-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections -fno-finite-math-only`. Мы сознательно не используем `-ffast-math`, потому что он ломает детерминизм клеточного автомата жидкости и YCoCg-зажим в TAA. Также нет `-march=native` — бинарник должен быть переносим между процессорами.
>
> Зависимости оформлены как 22 git-сабмодуля в каталоге `external/`: SDL3, volk, VMA, Jolt, Flecs, fastgltf, Draco, meshopt, miniaudio, fmt, glm, Tracy, fastgltf, imgui, rmlui, freetype, zstd, glaze, slang и другие. Все с закреплёнными SHA.
>
> Тестирование — это 14 ctest-наборов. Базовый уровень на текущий момент — 14 из 14 пройдены за 0.78 секунды в отладочной сборке и за 0.06 секунды в релизе — это эффект O3 и тонкого LTO. Дополнительно у нас есть Runtime Smoke — скрипт, который запускает приложение, генерирует шесть эталонных слоёв-захватов сцены и сравнивает их с эталонами. Сейчас это 6 из 6 captures за 1 секунду wall clock.
>
> Метрики производительности на эталонной сцене Voxel Laboratory: 110–130 кадров в секунду, время кадра 7–9 миллисекунд в отладочной сборке и 5–6 миллисекунд в релизе. Размер бинарника в релизе — 19 мегабайт, что на 73 процента меньше, чем 72 мегабайта в отладочной сборке. Ускорение FPS в релизе — от полутора до двух с половиной раз. Передаю слово коллеге.»

---

## 3. Понятия (10 терминов, чтобы понимать что говоришь)

| Термин | Что это в одном предложении |
|---|---|
| **C++26** | Последний стандарт C++ на 2026 год. Даёт `std::expected`, `std::simd`, модули, концепты. |
| **Vulkan 1.4** | Низкоуровневый графический API 2023 года. Explicit control, dynamic rendering, timeline semaphores. |
| **Clang 22.1.6** | Версия компилятора. На Windows = clang-cl.exe, на Linux = native clang 22. |
| **libc++** | Стандартная библиотека C++ от LLVM проекта. Альтернатива libstdc++ от GNU. |
| **CMake 4.0** | Система сборки. У нас 7 debug + 8 release пресетов для изоляции build trees. |
| **LTO (Link-Time Optimization)** | Оптимизация при линковке. `-flto=thin` — параллельный LTO. |
| **NDEBUG** | Макрос, отключающий `assert()`. В Release = ON, в Debug = OFF. |
| **`-ffast-math`** | Агрессивные математические оптимизации. Ломает NaN/Inf semantics, Inf-чувствительные алгоритмы. |
| **submodule** | Git-сабмодуль: внешний репозиторий, привязанный к конкретному SHA внутри нашего. |
| **ctest** | Утилита запуска тестов из CMake. Находится в `tests/CMakeLists.txt`. |

---

## 4. Что показывать на экране (если попросят)

**Демо 1 — ctest baseline (~30 секунд):**
```bash
cd /home/le1t/Projects/ProjectV
ctest --test-dir build/linux-clang-debug --output-on-failure
```
Ожидаемый результат: **14/14 tests passed**, время ~0.78s.

**Демо 2 — список пресетов (~10 секунд):**
```bash
cmake --list-presets=configure
```
Показывает 7 debug + 8 release entries.

**Демо 3 — release ELF size (~10 секунд):**
```bash
ls -lh build/linux-clang-release/bin/ProjectV
```
Показывает 19 MB (или около того).

**Демо 4 — submodule list (~20 секунд):**
```bash
git submodule status
```
Показывает 22 сабмодуля с закреплёнными SHA.

**Демо 5 — runtime smoke (~10 секунд):**
```bash
ls build/linux-clang-debug/lookdev-captures/
```
Показывает каталоги с `.bmp` + `.txt` sidecar парами.

---

## 5. Out of scope — куда отправлять вопросы

| Вопрос про… | Говори |
|---|---|
| Почему C++26, а не Rust/Zig/Go | «Это архитектурное решение le1t, попрошу его прокомментировать» |
| Почему Vulkan, а не OpenGL/DX12 | «Архитектурное обоснование — к le1t» |
| DOD layout / `alignas(16)` | «Это архитектурное решение le1t» |
| Walk controller / Jolt интеграция | «Это модуль Тиммейта 4, передам ему» |
| Voxel meshing / visibility cache | «Это модуль Тиммейта 2» |
| Тени / TAA / AOCC | «Это модуль Тиммейта 3» |
| Демо VoxelLab / ассеты / аудио | «Это модуль Тиммейта 5» |
| BUG-004 VoxelLab tremor | «Это TAA-scope, le1t расскажет детали» |
| Hot shader reload F5 | «Это к le1t, относится к рендереру» |

---

## 6. Если попросят «расскажите подробнее» (что вы можете раскрыть)

### Если спрашивают про libc++ миграцию:
> «Миграция прошла в коммите `c3faa65` (Tier 2.5). Причины: libstdc++ 16 на Linux не тянет `size_t` транзитивно (нужен явный `<cstddef>`), и не полная поддержка C++26 модулей. libc++ — header-only `<cstring>` для `std::mem*` через глобальный `-include cstring`. Сейчас обе платформы на libc++, build green.»

### Если спрашивают про Release presets:
> «8 release пресетов добавили 2026-06-14. Conservative policy: `-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections -fno-finite-math-only -Wl,--gc-sections`. Validation layers, Tracy, RenderDoc, Benchmarks — OFF. Test executables сохраняются (для ctest baseline).»

### Если спрашивают про `-ffast-math` почему не используем:
> «`-ffast-math` отключает IEEE 754 compliance: заменяет NaN на 0, убирает inf-проверки, переупорядочивает fp-операции. Это ломает Fluid CA determinism (распространение жидкости становится non-deterministic между сборками) и YCoCg clamp в TAA (clamp по компонентам требует строгого fp порядка). Один из наших принципов в `decisions.md §4` — reproducibility.»

### Если спрашивают про ctest:
> «14 наборов: `ProjectVTests` (math, voxel, materials), `AssetLoaderTests`, `MeshBakerTests`, `DracoDecoderTests`, `FrustumCullingTests` + `CFrustumCullingTests` (scalar vs C/AVX2), `SunShadowCascadeSplitsTests`, `BoxUvFixtureTests`, `MathTests`, `StringIdTests`, `ModuleSmoke`, `StdModuleProbe`. На release: 14/14 в 0.06s.»

### Если спрашивают про RuntimeSmoke:
> «`tools/linux/Invoke-ProjectVRuntimeSmoke.sh` запускает ProjectV, ждёт N кадров, делает 6 захватов (FINAL, SHDW, CSM, CTSH, AOCC, LOCL), проверяет sidecar metadata. На VoxelLab: 6/6 captures за 1 секунду wall clock. Это integration test, не unit test — проверяет, что весь pipeline от Vulkan init до GPU output работает.»

### Если спрашивают про метрики:
> «VoxelLab reference shot — статичная камера, все эффекты включены (TAA, CAS, CSM, AOCC, local light). Debug: 110-130 FPS, 7-9 мс. Release: 5-6 мс, +1.5-2.5× FPS. ELF 19 MB release vs 73 MB debug (verified `ls -lh 2026-06-15`), -73%. Per-pass timings на release: shadow 30 µs, graphics 76 µs, TAA 3 µs.»

---

## 7. Cheat-card для печати (1 страница A4)

```
┌────────────────────────────────────────────────────────────────────────┐
│              BRIEFER 1 — Стек и сборка (1:30)                          │
├────────────────────────────────────────────────────────────────────────┤
│ НАЧАЛО: "Добрый день, меня зовут [Имя Тимейта 1], я расскажу про      │
│          технологический стек и систему сборки проекта."               │
├────────────────────────────────────────────────────────────────────────┤
│ КЛЮЧЕВЫЕ ЦИФРЫ:                                                       │
│  • Clang 22.1.6, libc++ 16, CMake 4.0                                   │
│  • 7 debug + 8 release пресетов                                        │
│  • 22 git-сабмодуля с закреплёнными SHA                                │
│  • 14/14 ctest, 0.78s debug / 0.06s release                            │
│  • Runtime smoke 6/6 captures, 1s wall clock                           │
│  • 110-130 FPS VoxelLab debug, 5-6 мс release                          │
│  • ELF 19 MB release (-73% vs 73 MB debug, verified 2026-06-15)    │
├────────────────────────────────────────────────────────────────────────┤
│ ЧТО ГОВОРИТЬ:                                                          │
│  1. C++26, Vulkan 1.4, DOD, ECS                                        │
│  2. Clang 22 + libc++ 16 (миграция Tier 2.5)                           │
│  3. CMake presets, Release без -ffast-math                              │
│  4. 22 сабмодуля, ctest 14/14, smoke 6/6                              │
│  5. Метрики: 110-130 FPS, 19 MB release                                 │
├────────────────────────────────────────────────────────────────────────┤
│ OUT OF SCOPE → le1t: архитектура, DOD, выбор библиотек                 │
│              → T2: voxel, meshing                                       │
│              → T3: тени, TAA, AOCC                                      │
│              → T4: физика, walk                                         │
│              → T5: демо, ассеты, аудио                                  │
├────────────────────────────────────────────────────────────────────────┤
│ ЕСЛИ СПРОСЯТ ГЛУБЖЕ:                                                   │
│  • libc++ миграция: c3faa65, Tier 2.5                                  │
│  • Release presets: conservative, без -ffast-math, без -march=native    │
│  • ctest 14 наборов: ProjectV/Asset/Mesh/Draco/Frustum-C/...           │
│  • Runtime smoke: 6 captures FINAL/SHDW/CSM/CTSH/AOCC/LOCL             │
└────────────────────────────────────────────────────────────────────────┘
```

---

**Конец памятки.** Перед защитой: прочитать §2 вслух 3 раза с таймером, уложиться в 1:30 ± 5 секунд. Cheat-card [§7] распечатать.
