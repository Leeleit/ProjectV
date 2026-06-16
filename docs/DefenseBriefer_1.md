# Памятка Тиммейта 1 — Сборка и тестирование (говорит T1 Вступление)

**Участник:** [Имя Тимейта 1]
**Слот на сцене:** 0:00–0:45 (45 секунд) — T1 Вступление и проблема
**Твоя реальная компетенция:** Сборка и тестирование (CMake, ctest, runtime smoke, presets)
**Что НЕ твоё (к кому перенаправлять в Q&A):** архитектура/стек — к le1t; воксельный мир — к Тиммейту 2; рендеринг — к Тиммейту 3; физика/walk-контроллер — к Тиммейту 4; ассеты/аудио — к Тиммейту 5; все баги/хоткеи — к le1t

---

## 1. Шапка выступления

> «Здравствуйте. Меня зовут [Имя Тимейта 1], я начну — расскажу, кто мы, что за проект, и какую проблему решаем.»

---

## 2. Что говорить дословно (~80-100 русских слов, 0:45)

> «Здравствуйте. Мы команда "Черепашки Ninja", наш проект — воксельный игровой движок на базе Vulkan API и современного C++.
>
> **[Переход на второй слайд]**
>
> Проблема, которую мы решаем: разработчикам воксельных игр не хватает открытого, низкоуровневого и современного движка. Существующие варианты либо закрыты, либо слишком перегружены, а некоторые могут быть плохо оптимизированы и не давать полного контроля над ресурсами компьютера. Наша цель — создать быстрый, открытый движок, который выжимает максимум из видеокарты. Передаю слово другому участнику, он покажет демо и расскажет об архитектуре.»

---

## 3. Понятия (8 терминов, чтобы понимать что говоришь — общие для вступления)

| Термин | Что это |
|---|---|
| Воксельный движок | Игровой движок, где мир состоит из кубических объёмных элементов (вокселей) |
| Vulkan API | Низкоуровневый графический интерфейс, прямой доступ к видеокарте |
| C++26 | Стандарт языка C++ образца 2026 года |
| Открытый движок | Движок с публичным исходным кодом |
| Низкоуровневый контроль | Прямое управление памятью, шейдерами, пайплайнами видеокарты |
| Максимум из видеокарты | Оптимизация под конкретное железо, без лишних прослоек |
| Закрытые аналоги | Движки, чей исходный код не публикуется (Minecraft-подобные) |
| Перегруженные аналоги | Универсальные движки (Unity, Godot), которым не хватает низкого уровня |

---

## 4. Что показывать на экране

1. **Слайд 1 (титульный)** — название проекта, команда, логотип/QR.
2. **Слайд 2 (проблема)** — три категории существующих движков и почему они не подходят.
3. **Говорить глядя на аудиторию**, не в слайд.

---

## 5. Твоя настоящая компетенция (для Q&A): Сборка и тестирование

**Это то, что ты реально знаешь. На сцене ты говоришь про вступление, но на вопросы комиссии отвечаешь по своей компетенции.**

**Ключевые файлы:**
- `CMakePresets.json` — 12 configure-пресетов (8 debug + 4 release)
- `src/CMakeLists.txt` — root build, ~30 строк
- `tests/CMakeLists.txt` — 14 test executables
- `src/app/BenchmarkAutomation.cpp` — автобенчмарки (env: `PROJECTV_BENCHMARK_FRAMES`, `*_WARMUP_FRAMES`, `*_QUIT`, `*_LOG_EVERY`)
- `src/app/LookDevCaptureAutomation.cpp` — рантайм smoke (env: `PROJECTV_START_CAMERA_POSITION`, `*_CAMERA_LOOK`, `*_CAPTURE_VIEWS`, `*_WARMUP_FRAMES`, `*_INTERVAL_FRAMES`, `*_QUIT`)
- `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` и `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1` — обёртки smoke

**14 ctest-тестов (baseline 14/14, ~0.78s debug, ~0.06s release):**
- `ProjectVTests`, `ProjectVAssetTests`, `ProjectVMeshBakerTests`, `ProjectVDracoTests`, `ProjectVFrustumCullingTests`, `ProjectVCFrustumCullingTests`, `ProjectVSunShadowCascadeSplitsTests`, `ProjectVBoxUvFixtureTests`, `ProjectVMathTests`, `ProjectVStringIdTests`, `ProjectVModuleSmoke`, `ProjectVStdModuleProbe`, `ProjectVFluidCATests`, `ProjectVPresentModeTests`

**6/6 runtime smoke captures:** FINAL / SHDW / CSM / CTSH / AOCC / LOCL (эталонные debug-виды под `build/<preset>/lookdev-captures/<timestamp>/`).

**Sidecar metadata** — `.txt` рядом с `.bmp`, 60+ ключей: FPS, frame time, voxel counts, TAA state, shadow params и т.д.

**Hotkeys (для само-проверки):** F6 (SaveWorldSnapshot), F7 (LoadWorldSnapshot), C (CaptureScreenshot), B (CycleLightingDebugView), `B` циклически переключает debug-виды (FINAL → SHDW → CSM → CTSH → AOCC → LOCL).

**Build команды (запомни):**
- `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8`
- `ctest --test-dir build/linux-clang-debug --output-on-failure` → 14/14 passed
- `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh` → exit 0
- `cmake --preset linux-clang-release && cmake --build --preset linux-clang-release-build` → 19 MB ELF (vs 73 MB debug, -73%)

Подробнее — `docs/DefenseCompetency_FAQ.md §1` (textbook для Тиммейта 1).

---

## 6. Вне зоны ответственности (к кому перенаправлять в Q&A)

| Вопрос про… | Говори |
|---|---|
| C++26 / Vulkan 1.4 / DOD / SIMD / C-ядра | «К le1t» |
| Демо / FPS / HUD / сцена VoxelLab | «К le1t» |
| Воксельный мир / чанки / meshing / Jolt / статик-ассерты | «К Тиммейту 2» |
| Рендеринг / Vulkan / TAA / CSM / AOCC / шейдеры | «К Тиммейту 3» |
| Физика / walk-контроллер / Jolt / edge grace | «К Тиммейту 4» |
| Ассеты / аудио / snapshot / hot reload / meshopt | «К Тиммейту 5» |
| BUG-005 / баги / известные проблемы | «К le1t» |
| Hot shader reload / хоткеи (F1-F12, 1/2/3) | «К le1t» |
| Phase 4-9 / roadmap / планы | «К Тиммейту 4 (он закрывает)» |

---

**Конец памятки.** Перед защитой: прочитать §2 вслух 3 раза с таймером, уложиться в 0:45 ± 5 секунд. §5 прочитать отдельно, чтобы Q&A был уверенным.
