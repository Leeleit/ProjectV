# DefenseSpeakerNotes.md — Talking points для 6 участников защиты

**Дата защиты:** 2026-06-15
**Структура:** 6 секций по ~1:30 минут (≈220 слов каждая)
**Правило:** каждый участник читает свой раздел вслух, не импровизирует.
**le1t** говорит вступительное слово (2:00), заключение (0:30) и отвечает на все вопросы комиссии.
**Подробнее для каждого:** `docs/DefenseBriefer_{1..5}.md` (полные памятки с понятиями, Q&A, cheat-card).

---

## §1. Вступительное слово (le1t) — 2 минуты

**Ведущий:** Кадочников Лев Петрович
**Полный текст:** [`DefenseBriefer_le1t.md §2`](DefenseBriefer_le1t.md#2-вступление-verbatim-200) — читать дословно.

**Краткое содержание:**
- ProjectV — воксельный движок на C++26 + Vulkan 1.4.
- Проблема: нет современного открытого движка воксельной графики с DOD и контролем GPU.
- Цель: надёжный, расширяемый, измеримый фундамент.
- Метрика: 110–130 FPS, 7–9 мс кадр на RTX 3060 Ti.
- Три столпа: Vulkan 1.4 + compute, DOD (`alignas(16)`, чанки, SIMD), ECS через Flecs.
- Физика Jolt + voxel solver для walk, ассеты fastgltf/Draco/meshopt, аудио miniaudio.
- Честно: BUG-004 (VoxelLab tremor), BUG-005 (F5 VUID race) — задокументированы.
- Передаёт слово коллегам.

**Переход к Тиммейту 1:** «Передаю слово коллеге — он расскажет про технологический стек и систему сборки проекта.»

---

## §2. Стек и сборка — 1:30

**Участник:** Тиммейт 1
**Полный текст:** [`DefenseBriefer_1.md §2`](DefenseBriefer_1.md#2-что-говорить-verbatim-130-220-слов) — читать дословно.

**Краткое содержание:**
- C++26, Vulkan 1.4, DOD, ECS.
- Clang 22.1.6 (clang-cl на Windows, native clang 22 на Linux).
- libc++ 16 (миграция Tier 2.5, `c3faa65`).
- CMake 4.0: 7 debug + 8 release пресетов.
- Release policy: `-O3 -flto=thin -DNDEBUG` без `-ffast-math` без `-march=native`.
- 22 git-сабмодуля с закреплёнными SHA.
- 14/14 ctest baseline, 0.78s debug / 0.06s release.
- RuntimeSmoke 6/6 captures.
- Метрики: 110-130 FPS debug, 19 MB release ELF (-73%).

**Переход к Тиммейту 2:** «Теперь послушаем про то, как устроен воксельный мир и алгоритм жадного мешинга.»

---

## §3. Voxel-мир и meshing — 1:30

**Участник:** Тиммейт 2
**Полный текст:** [`DefenseBriefer_2.md §2`](DefenseBriefer_2.md#2-что-говорить-verbatim-130-220-слов) — читать дословно.

**Краткое содержание:**
- Чанки 8×8×8 = 512 вокселей, 1 байт/воксель, плотный массив.
- VoxelLab: 27 чанков (3×3×3 grid), генерация 200 мс.
- 5 материалов: Air, Glass, Fluid, FloorWhite, FloorGray.
- 3 solid для физики: Glass, FloorWhite, FloorGray.
- Greedy meshing (Лысенков): 6 проходов, 2D scan, 30-50% сокращение граней.
- Visibility cache: custom XOR-fold hash (Knuth-style multipliers + splitmix64-style avalanche), 2 уровня, cache hit = skip frustum cull, 8× ускорение.

**Переход к Тиммейту 3:** «Дальше — про рендеринг: тени, сглаживание, контактные тени и экспериментальный ray-marching.»

---

## §4. Тени, TAA, AOCC, ray-marching — 1:30

**Участник:** Тиммейт 3
**Полный текст:** [`DefenseBriefer_3.md §2`](DefenseBriefer_3.md#2-что-говорить-verbatim-130-220-слов) — читать дословно.

**Краткое содержание:**
- **CSM:** 4 каскада 2048×2048, lambda 0.80 near-biased, XY sphere fit.
- **CTSH:** voxel DDA к солнцу, max 5 единиц, 16 max steps.
- **AOCC:** 3 направления × 4 шага = 12 DDA, локальный forward-path occlusion, не SSAO.
- **TAA:** Halton(2,3) jitter, YCoCg clamp, 7 invalidation triggers, CAS sharpening.
- **Ray-march:** F6 toggle, Amanatides-Woo 3D DDA через packed voxel payload, OFF default.
- **B10G11R11_UFLOAT:** 2× bandwidth vs R16G16B16A16.
- **Glass не кастует тень:** зафиксировано в `decisions.md §15`.

**Переход к Тиммейту 4:** «Переходим к физике и контроллеру игрока — это модуль Jolt и voxel-решатель.»

---

## §5. Физика и walk controller — 1:30

**Участник:** Тиммейт 4
**Полный текст:** [`DefenseBriefer_4.md §2`](DefenseBriefer_4.md#2-что-говорить-verbatim-130-220-слов) — читать дословно.

**Краткое содержание:**
- Jolt Physics: MIT, SIMD, deterministic.
- **Voxel solver** авторитетный для walk, не Jolt `CharacterVirtual` (per `decisions.md §6`).
- 3 режима: walk / creative / spectator. F4 циклически, двойной Space ↔ walk ↔ creative.
- **Edge grace:** `kWalkEdgeGraceFrames = 4` фрейма + `kWalkFootSupportEdgeGraceScore = 0.2f` (НЕ 0.1 м), не дёргает игрока на тонких краях.
- **Sneak (Shift):** sampled top-plane, без false-stick к стене.
- **Auto-jump:** J toggle, F12 delay, по forward voxel.
- **Substepping в creative:** anti-tunneling для high-velocity.
- **Voxel raycast:** 3D DDA через чанки для placement/removal.
- **Hotkeys:** F4 cycle, двойной Space, J/F12, P, [ ], \`, `` ` ``.

**Переход к Тиммейту 5:** «И, наконец, описание демо-сцены Voxel Laboratory, ассетного конвейера и аудио. После этого я скажу заключительное слово, и мы перейдём к вопросам.»

---

## §6. Демо VoxelLab + ассеты + аудио — 1:30

**Участник:** Тиммейт 5
**Полный текст:** [`DefenseBriefer_5.md §2`](DefenseBriefer_5.md#2-что-говорить-verbatim-130-220-слов) — читать дословно.

**Краткое содержание:**
- VoxelLab: пол 18×18, стеклянный шар r=5, жидкость, 27 чанков.
- 27 чанков (3×3×3), генерация 200 мс.
- **Asset pipeline:** fastgltf → Draco decode → meshopt optimize → MeshBaker → VMA upload.
- **Manifest:** `PROJECTV_MODELS=path.glb@x,y,z;...` env var, snap above ground.
- **miniaudio:** header-only MIT, PipeWire → PulseAudio.
- **Playlist:** scan каждые 5 секунд, MP3 only (WAV/FLAC тихо игнорируются, case-insensitive ext check).
- **Audio hotkeys:** Q play/pause, E stop, 7/8 vol, 9/0 next/prev.
- **Hot shader reload (F5):** `cmake --target Shaders` + pipeline recreate.
- **Sidecar:** .txt рядом с .bmp, 60+ ключей metadata.

**Переход к le1t:** «Передаю слово ведущему для заключительной части.»

---

## Заключительное слово (le1t) — 0:30

**Полный текст:** [`DefenseBriefer_le1t.md §3`](DefenseBriefer_le1t.md#3-закрытие-verbatim-030) — читать дословно.

**Краткое содержание:**
> «Подводя итог: ProjectV достиг поставленных целей MVP. У нас есть рабочий фундамент воксельного движка, 12 ctest-сьютов с baseline 14/14, runtime smoke 6/6 captures, метрики в sidecar, и воспроизводимая сборка на Windows и Linux. Открыто документируем техдолг и roadmap Phase 4–9. Готовы к вопросам.»

**Переход к Q&A:** «Готовы ответить на ваши вопросы.»

---

## Памятка для участников 1-5

- **Читайте дословно**, не импровизируйте.
- Не пытайтесь объяснить, что не сказано в вашем разделе.
- На вопросы комиссии отвечает le1t.
- Если не знаете ответа: «Этот вопрос лучше адресовать le1t».
- Держите темп: ~220 слов = ~1:30 минуты.
- Говорите в микрофон, смотрите на комиссию, не на экран.
- Если видите cue-карту le1t («сворачивай», «30 секунд») — заканчивайте.
- Если забыли текст — посмотрите на cheat-card (распечатан, на столе).

## Памятка для le1t

- Cue-карты переходов: [`DefenseScript.md §2`](DefenseScript.md#2-подробные-cue-карты-и-тексты-переходов).
- Q&A-карта 30 вопросов: [`DefenseBriefer_le1t.md §4`](DefenseBriefer_le1t.md#4-qa-карта-30-вероятных-вопросов).
- Полный reference алгоритмов: [`DefenseAlgorithms.md`](DefenseAlgorithms.md).
- Готовые ответы на 15+ вопросов: [`DefenseFAQ.md`](DefenseFAQ.md).
- Если тиммейт растерялся — мягко подхватить, продолжить.
- Если вопрос вне плана — отвечать самому, иначе «уточню письменно».

---

**Связанные документы:**
- `docs/DefenseScript.md` — 10-мин таймлайн, чеклисты, что делать при сбоях.
- `docs/DefenseBriefer_le1t.md` — verbatim вступление/закрытие + Q&A-карта для le1t.
- `docs/DefenseBriefer_{1..5}.md` — verbatim тексты и cheat-card'ы для тиммейтов.
- `docs/DefenseAlgorithms.md` — полный reference всех 23 алгоритмов.
- `docs/DefenseFAQ.md` — готовые ответы на типовые вопросы комиссии.
- `docs/DefenseReport.md` — формальный итоговый отчёт.
- `docs/DefenseDemoScript.md` — сценарий демо с HUD-командами.
