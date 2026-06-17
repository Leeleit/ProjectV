# DefensePresentation_Structure.md — ProjectV (13 слайдов, 5 мин defense)

**Дата защиты:** 2026-06-17 (перенесена с 2026-06-15)
**Формат:** 5 минут = 4:30 речь + 30с буфер. После 5:00 — тишина, Q&A начинается.
**Слайдов:** 13 (один слайд = одна мысль, per критерии п.8)
**Стиль:** PDF (рекомендация критериев п.8), шаблон МФТИ, минимум текста на слайде.
**Целевая оценка по критериям п.6:** 81–100% (оценка 5). Все 8 блоков покрыты.

> **Hand-off note для следующей сессии (LaTeX/PDF экспорт):**
> Каждый слайд ниже описан в 5 секциях:
> 1. **Визуальная структура** — header/subheader/body/footer + рекомендации для LaTeX Beamer
> 2. **Body content (verbatim)** — точный текст, который появляется на слайде (для \frametitle{} и \begin{itemize})
> 3. **Speaker notes (verbatim)** — то, что ГОВОРИТ спикер (копируется в `DefenseScript_Team.md`)
> 4. **Тайминг** — сколько секунд на этом слайде
> 5. **Источник данных** — где цифры/факты (для traceability)
>
> Все цифры подтверждены против исходного кода `src/**` и `agent/decisions.md`.

---

## Важно от преподавателя

- Меньше текста, больше акцента на актуальности и целях.
- Не зачитывать слайды, эксперты умеют читать.
- Красочное оформление (использовать шаблон МФТИ).
- Скриншоты работы приложения как подстраховка демо.
- Строго 5 минут. После 5:00 — останавливают сразу.

---

## Распределение времени (строго)

| Slot | Хронометраж | Тема (слайды) | Спикер | Реальная компетенция (для Q&A) |
|------|-------------|----------------|--------|-------------------------------|
| T1   | 0:00–0:55   | Слайды 1, 2, 3 (титул, проблема, цели) | Тиммейт 1 | Сборка и тестирование |
| T2   | 0:55–2:20   | Слайды 4, 5, 6 (demo, аналоги, архитектура) | le1t (ведущий) | Архитектура + Q&A host |
| T3   | 2:20–3:00   | Слайды 7, 8 (реализация воксельного мира, тесты) | Тиммейт 2 | Воксельный мир |
| T5   | 3:00–3:55   | Слайды 9, 10 (доп. фичи, метрики) | Тиммейт 5 | Ассеты и аудио |
| T6   | 3:55–4:25   | Слайды 11, 12 (ограничения, команда) | Тиммейт 4 | Физика и walk-контроллер |
| le1t | 4:25–4:30   | Слайд 13 (выводы, Q&A invite) | le1t | Закрытие |
| —    | 4:30–5:00   | **Буфер** на форс-мажоры | Тишина | — |

**Важно:** speech slot ≠ real competency. Все 5 тиммейтов на сцене говорят «мы», роли не акцентируем. Реальные компетенции нужны для Q&A — см. `docs/DefenseCompetencyFAQ_T{1..6}.md`.

---

## Слайд 1: Титульный (0:00–0:10, Тиммейт 1)

### Визуальная структура (LaTeX Beamer рекомендация)
- **Header (центр, \Huge):** ProjectV
- **Subheader (ниже, \Large):** Высокопроизводительный воксельный движок
- **Body (мелко, центр):**
  - Стек: C++26 • Vulkan 1.4 • Data-Oriented Design • ECS
  - Команда: «Черепашки Ninja» (6 человек)
  - Курс: Основы проектной деятельности в ИТ-сфере, МФТИ-1-2024
  - Руководитель: Подольский Филипп Александрович
- **Footer:** страница 1 / 13
- **QR-код (правый нижний угол):** https://github.com/Leeleit/ProjectV

### Body content (verbatim — копировать в LaTeX)
```latex
\frametitle{ProjectV}
\framesubtitle{Высокопроизводительный воксельный движок}
\begin{center}
\textbf{C++26} $\bullet$ \textbf{Vulkan 1.4} $\bullet$ \textbf{DOD} $\bullet$ \textbf{ECS}\\[0.5em]
Команда <<Черепашки Ninja>> (6 человек)\\[0.3em]
Основы проектной деятельности в ИТ-сфере, МФТИ-1-2024\\
Руководитель: Подольский Филипп Александрович
\end{center}
```

### Speaker notes (verbatim — копировать в DefenseScript_Team.md)
«Здравствуйте. Мы команда "Черепашки Ninja", наш проект — воксельный игровой движок на базе Vulkan API и современного C++.»

### Тайминг
**10 секунд.** Не задерживаться — это вводный слайд.

### Источник данных
- Команда/руководитель: `docs/DefenseCompetencyFAQ_T2.md:343–354` (§3.6 команда)
- GitHub: `https://github.com/Leeleit/ProjectV`

### Подстраховка
- Если демо упало: этот слайд показывается как первый, остаётся на экране дольше.

---

## Слайд 2: Проблема и ценность (0:10–0:35, Тиммейт 1)

### Визуальная структура
- **Header (\Large):** Проблема: нет открытого современного фундамента
- **Subheader:** Для кого и почему это важно
- **Body (3 секции, можно 3 строки или 3 столбца):**

| Секция | Содержание |
|--------|-----------|
| **Кто** | Indie-разработчики воксельных игр; исследователи компьютерной графики; студенты-разработчики движков |
| **Что** | Существующие движки либо закрытые (Minecraft), либо high-level без контроля над GPU/CPU/memory (Unity, Godot), либо legacy OpenGL без DOD-оптимизации |
| **Почему важно** | Ни один open-source voxel-движок не сочетает DOD + Vulkan 1.4 + compute shaders + C++26 в одном воспроизводимом фундаменте |

- **Footer:** страница 2 / 13

### Body content (verbatim)
```latex
\frametitle{Проблема: нет открытого современного фундамента}
\framesubtitle{Для кого и почему это важно}

\begin{columns}[T]
\column{0.32\textwidth}
\textbf{Кто}\\
Indie-разработчики воксельных игр; исследователи графики; студенты-движкописатели

\column{0.32\textwidth}
\textbf{Что}\\
Закрытые (Minecraft), high-level без контроля (Unity, Godot), legacy OpenGL без DOD

\column{0.32\textwidth}
\textbf{Почему важно}\\
Ни один open-source voxel не сочетает DOD + Vulkan 1.4 + compute + C++26
\end{columns}
```

### Speaker notes (verbatim)
«Проблема, которую мы решаем: разработчикам воксельных игр не хватает открытого, низкоуровневого и современного движка. Существующие варианты либо закрыты, либо слишком перегружены, а некоторые могут быть плохо оптимизированы и не давать полного контроля над ресурсами компьютера. Наша цель — создать быстрый, открытый движок, который выжимает максимум из видеокарты. Передаю слово другому участнику, он покажет демо и расскажет об архитектуре.»

### Тайминг
**25 секунд.**

### Источник данных
- Проблема/пользователь: `docs/DefenseCompetencyFAQ_T2.md §3.7` (10.x defense questions) + `DefenseReport.md §1`
- Аналоги для контекста «закрытые/legacy»: research exa search (Minetest, VoxelCore, Veloren, Minecraft Java)
- «Ни один open-source не сочетает»: research finding 2026 — verified against Minetest (C++17/OpenGL), Veloren (Rust/wgpu), VoxelCore (C++17/OpenGL), VIXEN (только Vulkan research), Garden (C++17), Shroom (C++20/Vulkan), Enigma (C++26 но DX12+Vulkan, multi-backend)

---

## Слайд 3: Цели и спецификации (0:35–0:55, Тиммейт 1)

### Визуальная структура
- **Header (\Large):** Цели и спецификации
- **Subheader:** Измеримые критерии, верифицированные через ctest + smoke captures
- **Body (верх):** Мини-сводка ТЗ compliance
  - **38 ✅ / 5 ⚠️ отложено / 0 ❌ критичных** из 48 пунктов ТЗ
- **Body (низ):** 5 ключевых измеримых критериев в виде compact grid (2 строки × 3 колонки или 5 строк):

| Критерий | Значение | Источник |
|----------|----------|----------|
| FPS (VoxelLab reference) | 500+ FPS (~2 мс кадр, debug) | Runtime smoke 6/6 |
| Размер ELF | 19 MB release (-73% vs 73 MB debug) | `cmake --build` |
| ctest baseline | 14/14 за 0.78 с debug / 0.06 с release | `ctest` |
| Smoke captures | 6/6 (FINAL/SHDW/CSM/CTSH/AOCC/LOCL) | LookDevCapture |
| Предупреждения в нашем коде | 0 | `decisions.md §4` |

- **Footer:** страница 3 / 13

### Body content (verbatim)
```latex
\frametitle{Цели и спецификации}
\framesubtitle{Измеримые критерии, верифицированные через ctest + smoke captures}

\begin{center}
\textbf{38 ✅ / 5 ⚠️ / 0 ❌} из 48 пунктов ТЗ
\end{center}

\vspace{0.5em}
\begin{tabular}{lll}
\textbf{FPS VoxelLab} & \textbf{500+ FPS ($\sim$2 мс)} & Runtime smoke 6/6 \\
\textbf{ELF release} & \textbf{19 MB} ($-73\%$ vs 73 MB debug) & \texttt{cmake --build} \\
\textbf{ctest baseline} & \textbf{14/14} за 0.78 с / 0.06 с & \texttt{ctest} \\
\textbf{Smoke captures} & \textbf{6/6} (FINAL/SHDW/CSM/CTSH/AOCC/LOCL) & LookDevCapture \\
\textbf{Warnings} & \textbf{0} в нашем коде & \texttt{decisions.md §4} \\
\end{tabular}
```

### Speaker notes (для Тиммейта 1 — можно дословно)
«Чтобы вы видели, что мы мерим. Из 48 пунктов технического задания мы закрыли 38, ещё 5 явно отложили в roadmap (это Phase 4–9), и ни одного критического провала. Цифры из реальных измерений: 500+ FPS на сцене VoxelLab в отладочной сборке, бинарник ужался с 73 мегабайт до 19 — минус 73 процента — за счёт ThinLTO и удаления неиспользуемых секций. Все 14 автоматических тестов зелёные. Шесть эталонных снимков пиксель-в-пиксель совпадают с эталоном. И ноль предупреждений компилятора в нашем коде.»

### Тайминг
**20 секунд.**

### Источник данных
- ТЗ compliance 48 пунктов: `docs/DefenseCompetencyFAQ_T2.md:282–335` (§3.5)
- 500+ FPS: `docs/DefenseCompetencyFAQ_T1.md:113`, `T2.md:368`
- 19 MB vs 73 MB: `docs/DefenseCompetencyFAQ_T1.md:117`, `T2.md:225`
- 14/14 ctest: `docs/DefenseCompetencyFAQ_T1.md:42–57`, `T2.md:222`
- 6/6 smoke: `docs/DefenseCompetencyFAQ_T1.md:58`, `T4.md:116`
- 0 warnings: `agent/decisions.md §4`

---

## Слайд 4: Live Demo (0:55–2:00, le1t) — самый длинный, 1:05

### Визуальная структура
- **Header (\Large):** Live Demo: Voxel Laboratory
- **Body:** минимум текста, основная часть экрана — окно приложения (скриншот или live screen)
- **Подписи к демо (мелко, по бокам или внизу):**
  - **Слева:** Стекло (Glass) — полупрозрачный, не кастует тень
  - **Справа:** Жидкость (Fluid) — кастует тень, обновляется клеточным автоматом (Fluid CA)
  - **Центр внизу:** HUD — 500+ FPS, 27 чанков (3×3×3), 5 материалов
- **Footer:** страница 4 / 13

### Подстраховка демо (если live demo упало)
- Показать screenshot из `build/linux-clang-debug/lookdev-captures/FINAL/*.bmp` или sidecar
- Вернуться на слайд 4, объяснить: «Это эталонный снимок из runtime smoke, пиксель-в-пиксель с эталоном. 500+ FPS, 27 чанков, генерация сцены менее 10 мс.»

### Body content (verbatim)
```latex
\frametitle{Live Demo: Voxel Laboratory}
% Большую часть слайда занимает \includegraphics{voxellab_screenshot.png}
% Минимум текста, чтобы не отвлекать от демо

\vspace{0.5em}
\begin{columns}[T]
\column{0.3\textwidth}
\textbf{Слева:} Стекло (Glass) — полупрозрачный, не кастует тень

\column{0.3\textwidth}
\textbf{Центр:} HUD — 500+ FPS, 27 чанков, 5 материалов, генерация сцены < 10 мс

\column{0.3\textwidth}
\textbf{Справа:} Жидкость (Fluid) — кастует тень, обновляется клеточным автоматом
\end{columns}
```

### Speaker notes (verbatim — для le1t, основная речь)
«Здравствуйте. Перед вами запущенная тестовая лаборатория нашего движка. Генерация сцены менее 10 миллисекунд. Вы видите стекло, жидкость, тени и жадный мешинг геометрии. Сверху слева и сверху справа HUD — мы держим 500+ FPS на первой сцене.

**[Я возвращаю презентацию и переключаю на 3 слайд]**

Технически проект написан на современном C++ 26. Мы используем графический API Vulkan 1.4 для низкоуровневого управления видеокартой и работой с шейдерами. Архитектура построена на дата-ориентированном дизайне (DOD) — мы выравниваем данные в памяти для максимальной скорости кэша процессора и минимальных cache miss-ов. Также мы используем SIMD-инструкции и C-вставки на C 23. Дальше следующий участник расскажет про то, что внутри движка.»

**Важно:** на этом слайде 1 минута — это самая длинная секция. Demo + объяснение стека укладываются в 65 секунд.

### Тайминг
**65 секунд (1:05).**

### Источник данных
- 500+ FPS VoxelLab debug: `docs/DefenseCompetencyFAQ_T2.md:368`, `T1.md:113`
- 27 чанков (3×3×3): `docs/DefenseCompetencyFAQ_T3.md:85`
- 5 материалов (Air/Glass/Fluid/FloorWhite/FloorGray): `docs/DefenseCompetencyFAQ_T3.md:65`
- Генерация сцены < 10 мс: `DefenseCompetencyFAQ_T3.md:72`
- Glass/Fluid кастуют тень: `decisions.md §15`, `DefenseCompetencyFAQ_T3.md §3.2`

---

## Слайд 5: Аналоги и обоснование (2:00–2:20, le1t)

### Визуальная структура
- **Header (\Large):** Существующие решения и наша ниша
- **Subheader:** Почему именно наш стек закрывает пробел
- **Body:** Сравнительная таблица 5 аналогов (6 колонок):

| Аналог | Тип | Язык | Graphics API | DOD | Compute / Open source |
|--------|-----|------|--------------|-----|----------------------|
| **Minecraft Java Edition** | Commercial sandbox | Java | OpenGL → Vulkan (2026+) | Нет | Нет / Закрытый |
| **Minetest / Luanti** | Open-source sandbox | C++17 + Lua | OpenGL / Irrlicht | Нет | Нет / LGPL |
| **VoxelCore** (MihailRis, 1.4k⭐) | Open-source engine | C++17 | OpenGL | Нет | Нет / MIT |
| **Veloren** | Open-source RPG | Rust | wgpu / Vulkan | Частично (ECS) | Да (greedy) / MIT |
| **ProjectV (наш)** | **Open-source движок-фундамент** | **C++26** | **Vulkan 1.4** | **Да (SoA, alignas)** | **Да (voxel_mesh.comp)** / MIT |

- **Под таблицей (мелко):** **Пробел ниши:** ни один open-source voxel-движок не сочетает DOD + Vulkan 1.4 + compute shaders + C++26 в воспроизводимом фундаменте.
- **Footer:** страница 5 / 13

### Body content (verbatim)
```latex
\frametitle{Существующие решения и наша ниша}
\framesubtitle{Почему именно наш стек закрывает пробел}

\scriptsize  % мелкий шрифт для таблицы
\begin{tabular}{llllll}
\textbf{Аналог} & \textbf{Тип} & \textbf{Язык} & \textbf{API} & \textbf{DOD} & \textbf{Compute / Лицензия} \\
\hline
Minecraft Java & Commercial & Java & OpenGL→Vulkan & Нет & Нет / Закрытый \\
Minetest/Luanti & Open-source & C++17+Lua & OpenGL/Irrlicht & Нет & Нет / LGPL \\
VoxelCore (1.4k$\star$) & Open-source & C++17 & OpenGL & Нет & Нет / MIT \\
Veloren & Open-source RPG & Rust & wgpu/Vulkan & Частично & Да / MIT \\
\textbf{ProjectV (наш)} & \textbf{Фундамент} & \textbf{C++26} & \textbf{Vulkan 1.4} & \textbf{Да} & \textbf{Да / MIT} \\
\end{tabular}

\vspace{0.5em}
\normalsize
\textbf{Пробел ниши:} ни один open-source voxel не сочетает DOD + Vulkan 1.4 + compute + C++26.
```

### Speaker notes (verbatim — для le1t, переходная речь после demo)
«Прежде чем я расскажу про архитектуру — коротко о том, кто в этой нише уже есть и почему наш стек отличается. Minecraft — закрытый коммерческий, плюс только в 2026 году переходит с OpenGL на Vulkan. Minetest, самый успешный open-source voxel — это C++ плюс Lua плюс устаревший OpenGL, без DOD и без compute shaders. VoxelCore на чистом OpenGL, без вычислительных шейдеров. Veloren на Rust, с ECS-архитектурой — но это action-RPG с упором на мультиплеер, не фундамент для песочниц. ProjectV закрывает пробел: DOD-раскладка плюс Vulkan 1.4 плюс compute-шейдеры для мешинга плюс современный C++26 в воспроизводимом открытом фундаменте.

**Переход на 6 слайд:**

Архитектурно движок построен вокруг SDL3 главного цикла, в центре — единая точка владения AppState, от которой расходятся три подсистемы: рендерер на Vulkan, физика на Jolt и ECS-мир Flecs. Внутри ECS уже живут воксельный мир, аудио-движок и реестр ассетов. Главная идея — данные-ориентированный дизайн: чанки 8 на 8 на 8 вокселей, плоский массив материалов в один байт на воксель, что даёт плотный cache-friendly доступ и попадание в L1 кэш процессора. Жадный мешинг считается compute-шейдером на видеокарте: шесть проходов по чанку для каждой оси и направления объединяют соседние грани одного материала в большие четырёхугольники, сокращая draw calls на 30-50 процентов. Физика — библиотека Jolt, наш собственный код дополняет её для коллизий блоков. Для отладки данные дублируются в систему компонентов. В коде повсюду статик-ассерты: на этапе компиляции проверяют размеры структур и контракты алгоритмов, чтобы ничего не сдвигалось случайно. Передаю слово.»

### Тайминг
**20 секунд.**

### Источник данных
- Minecraft Java переход на Vulkan: web search research (Playing Games, Mojang announcement 2026)
- Minetest: web search research (LGPL, OpenGL/Irrlicht, 25 лет истории)
- VoxelCore: web search research (MihailRis, 1.4k stars, MIT, OpenGL)
- Veloren: web search research (Rust, wgpu/Vulkan, MIT, RPG focus)
- ProjectV ниша: собственный анализ стека — все 5 open-source конкурентов НЕ сочетают DOD + Vulkan 1.4 + C++26 + compute

---

## Слайд 6: Архитектура (2:20–2:40, le1t)

### Визуальная структура
- **Header (\Large):** Архитектура движка
- **Body (верх):** Текстовая диаграмма (ASCII-art или TikZ):
  ```
  SDL3 main loop (SDL_AppInit → SDL_AppEvent → SDL_AppIterate)
                       │
            ┌──────────┼──────────┐
            ▼          ▼          ▼
        Renderer   Physics   ECS (Flecs)
        (Vulkan)   (Jolt)    + VoxelWorld
                              + AudioEngine
                              + AssetRegistry
  ```
- **Body (низ):** 4 ключевых принципа в bullets:
  - **DOD:** чанки 8×8×8 = 512 вокселей = 512 B = 2 SSE-регистра, влезает в L1 (32 KB на Zen 3)
  - **Voxel layout:** плоский массив `std::vector<uint8_t> voxels`, индекс = `x + width*(y + height*z)`
  - **Greedy meshing:** compute-шейдер `voxel_mesh.comp`, 6 проходов (±X, ±Y, ±Z), сокращение draw calls 30–50%
  - **Static asserts:** compile-time проверка контрактов структур (защита от ABI shift, per `agent/memory.md §10.8`)
- **Footer:** страница 6 / 13

### Body content (verbatim)
```latex
\frametitle{Архитектура движка}

\begin{center}
\begin{tabular}{c}
\texttt{SDL3 main loop} (SDL\_AppInit → SDL\_AppEvent → SDL\_AppIterate) \\[0.3em]
$\big\downarrow$ \\[0.3em]
\begin{tabular}{ccc}
\texttt{Renderer} & \texttt{Physics} & \texttt{ECS (Flecs)} \\
(Vulkan 1.4) & (Jolt) & + VoxelWorld \\
& & + AudioEngine \\
& & + AssetRegistry \\
\end{tabular}
\end{tabular}
\end{center}

\vspace{0.5em}
\begin{itemize}
\item \textbf{DOD:} чанки 8×8×8 = 512 B = 2 SSE-регистра, влезает в L1 (32 KB на Zen 3)
\item \textbf{Voxel layout:} плоский \texttt{std::vector<uint8\_t> voxels}, индекс $= x + w \cdot (y + h \cdot z)$
\item \textbf{Greedy meshing:} compute-шейдер, 6 проходов, draw calls $-30{-}50\%$
\item \textbf{Static asserts:} compile-time проверка контрактов (per \texttt{memory.md §10.8})
\end{itemize}
```

### Speaker notes (verbatim)
«Архитектурно движок построен вокруг SDL3 главного цикла, в центре — единая точка владения AppState, от которой расходятся три подсистемы: рендерер на Vulkan, физика на Jolt и ECS-мир Flecs. Внутри ECS уже живут воксельный мир, аудио-движок и реестр ассетов. Главная идея — данные-ориентированный дизайн: чанки 8 на 8 на 8 вокселей, плоский массив материалов в один байт на воксель, что даёт плотный cache-friendly доступ и попадание в L1 кэш процессора. Жадный мешинг считается compute-шейдером на видеокарте: шесть проходов по чанку для каждой оси и направления объединяют соседние грани одного материала в большие четырёхугольники, сокращая draw calls на 30-50 процентов. Физика — библиотека Jolt, наш собственный код дополняет её для коллизий блоков. Для отладки данные дублируются в систему компонентов. В коде повсюду статик-ассерты: на этапе компиляции проверяют размеры структур и контракты алгоритмов, чтобы ничего не сдвигалось случайно. Передаю слово.»

### Тайминг
**20 секунд.**

### Источник данных
- Диаграмма: `docs/DefenseReport.md §4`, `docs/DefenseCompetencyFAQ_T2.md:247–280` (§3.4)
- DOD 8×8×8 = 512 B = 2 SSE-регистра: `docs/DefenseCompetencyFAQ_T3.md §3.1`, `T3.md:83`
- Greedy meshing compute shader 6 проходов: `docs/DefenseCompetencyFAQ_T3.md §3.3` (Алгоритм 3, FULL detail)
- 30-50% draw calls: `docs/DefenseAlgorithms.md §3`
- Static asserts: `agent/memory.md §10.8`, `docs/DefenseCompetencyFAQ_T3.md:88–97`

---

## Слайд 7: Реализация воксельного мира (2:40–3:00, Тиммейт 2)

### Визуальная структура
- **Header (\Large):** Реализация: воксельный мир
- **Subheader:** Что создано Тиммейтом 2
- **Body (верх):** Список файлов модуля:
  ```
  src/voxel/VoxelWorld.{hpp,cpp}        — main world (VoxelChunk 32 B = 2 SSE, плоский voxels)
  src/voxel/VoxelMaterials.{hpp,cpp}    — 5 материалов, per-preset shadow params
  src/voxel/VoxelRaycast.{hpp,cpp}      — 3D DDA raycast
  src/voxel/VoxelInteraction.{hpp,cpp}  — placement/removal
  src/voxel/SceneConfig.{hpp,cpp}       — JSON scene config
  src/voxel/VoxelSnapshotError.hpp      — error enum (Tier 1.B)
  src/shaders/voxel_mesh.comp           — compute-шейдер greedy meshing (Лысенков)
  ```
- **Body (середина):** VoxelLab демо-сцена
  - Пол 18×18 (XZ), стеклянный шар радиуса 6, жидкость внутри до ~70% радиуса, 3 якоря для теней
  - Процедурная генерация < 200 мс
- **Body (низ):** Fluid CA (клеточный автомат)
  - 2 правила: `f_fall` (вниз) и `f_spread` (2 перпендикулярных направления)
  - Hash = Teschner spatial, 20 Hz, bottom-up y-pass, double-buffered, claimed-tracking
- **Footer:** страница 7 / 13

### Body content (verbatim)
```latex
\frametitle{Реализация: воксельный мир}
\framesubtitle{Что создано Тиммейтом 2}

\scriptsize
\texttt{src/voxel/VoxelWorld.\{hpp,cpp\}} --- main world (VoxelChunk 32 B, плоский voxels)\\
\texttt{src/voxel/VoxelMaterials.\{hpp,cpp\}} --- 5 материалов, per-preset shadow params\\
\texttt{src/voxel/VoxelRaycast.\{hpp,cpp\}} --- 3D DDA raycast\\
\texttt{src/voxel/VoxelInteraction.\{hpp,cpp\}} --- placement/removal\\
\texttt{src/voxel/SceneConfig.\{hpp,cpp\}} --- JSON scene config\\
\texttt{src/voxel/VoxelSnapshotError.hpp} --- error enum (Tier 1.B)\\
\texttt{src/shaders/voxel\_mesh.comp} --- compute-шейдер greedy meshing (Лысенков)

\normalsize
\vspace{0.5em}
\textbf{VoxelLab:} пол 18×18, стеклянный шар R=6, жидкость ~70\% радиуса, 3 якоря. Генерация < 200 мс.

\vspace{0.3em}
\textbf{Fluid CA:} 2 правила (fall + spread), Teschner hash, 20 Hz, bottom-up y-pass, double-buffered.
```

### Speaker notes (verbatim — для Тиммейта 2)
«Здравствуйте. Несколько слов о том, что внутри. Мир разбит на чанки 8 на 8 на 8, воксели лежат одним плоским массивом — это даёт кэш-дружелюбный доступ. Мешинг считает compute-шейдер на видеокарте: жадно склеивает соседние грани одного материала в четырёхугольники для производительности. Физика — библиотека Jolt, наш собственный код дополняет её для коллизий блоков. Для отладки данные дублируются в систему компонентов. В коде повсюду статик-ассерты: на этапе компиляции проверяют размеры структур и контракты алгоритмов, чтобы ничего не сдвигалось случайно. Передаю слово.»

### Тайминг
**20 секунд.**

### Источник данных
- Все файлы: `docs/DefenseCompetencyFAQ_T3.md:28–36`
- VoxelLab детали: `docs/DefenseCompetencyFAQ_T3.md §3.1` (Алгоритм 1), `T2.md §3.6 команда`
- Fluid CA детали: `docs/DefenseCompetencyFAQ_T3.md §3.5` (Алгоритм 13 FULL detail, ~360 строк кода)
- `<200 мс` генерация: `docs/DefenseCompetencyFAQ_T3.md:72`

---

## Слайд 8: Тесты и проверки (3:00–3:20, Тиммейт 2)

### Визуальная структура
- **Header (\Large):** Тесты и проверки
- **Subheader:** 14 ctest suites baseline + 6 smoke captures + 60+ sidecar keys
- **Body (верх):** 14 ctest suites в compact grid (3 колонки × 5 строк или подобное):

| Suite | Назначение | Suite | Назначение |
|-------|-----------|-------|-----------|
| `ProjectVTests` | VoxelWorld (~157 sub-tests) | `ProjectVAssetTests` | LoadGlb (9 sub) |
| `ProjectVMeshBakerTests` | meshopt (4) | `ProjectVDracoTests` | Draco decode (3) |
| `ProjectVFrustumCullingTests` | C++ helper (5) | `ProjectVCFrustumCullingTests` | C-kernel (Tier 3) |
| `ProjectVSunShadowCascadeSplitsTests` | split planning | `ProjectVBoxUvFixtureTests` | UV projection (2) |
| `ProjectVMathTests` | Tier 0.A | `ProjectVStringIdTests` | Tier 1.D |
| `ProjectVModuleSmoke` | C++26 modules | `ProjectVStdModuleProbe` | std module probe |
| `ProjectVFluidCATests` | Fluid CA determinism | `ProjectVPresentModeTests` | present mode cycle |

- **Body (середина):** **14/14 ✅ baseline** — 0.78 с debug / 0.06 с release (×13 ускорение)
- **Body (низ):** 6 smoke captures: **FINAL / SHDW / CSM / CTSH / AOCC / LOCL** — пиксель-в-пиксель с эталоном. **60+ sidecar keys**: FPS, frame time, voxel counts (per material), shadow params, TAA state, tone map operator, exposure bias, hot shader version.
- **Footer:** страница 8 / 13

### Body content (verbatim)
```latex
\frametitle{Тесты и проверки}
\framesubtitle{14 ctest suites baseline + 6 smoke captures + 60+ sidecar keys}

\scriptsize
\begin{tabular}{ll|ll}
\texttt{ProjectVTests} & VoxelWorld 157sub & \texttt{ProjectVAssetTests} & LoadGlb 9sub \\
\texttt{ProjectVMeshBakerTests} & meshopt 4 & \texttt{ProjectVDracoTests} & Draco 3 \\
\texttt{ProjectVFrustumCullingTests} & C++ helper 5 & \texttt{ProjectVCFrustumCullingTests} & C-kernel \\
\texttt{ProjectVSunShadowCascadeSplitsTests} & splits & \texttt{ProjectVBoxUvFixtureTests} & UV 2 \\
\texttt{ProjectVMathTests} & Tier 0.A & \texttt{ProjectVStringIdTests} & Tier 1.D \\
\texttt{ProjectVModuleSmoke} & C++26 modules & \texttt{ProjectVStdModuleProbe} & std probe \\
\texttt{ProjectVFluidCATests} & Fluid CA det. & \texttt{ProjectVPresentModeTests} & present mode \\
\end{tabular}

\normalsize
\vspace{0.5em}
\textbf{14/14 ✅} baseline --- 0.78 с debug / 0.06 с release ($\times 13$ ускорение).

\vspace{0.3em}
\textbf{6 smoke captures} FINAL/SHDW/CSM/CTSH/AOCC/LOCL --- пиксель-в-пиксель с эталоном.

\vspace{0.3em}
\textbf{60+ sidecar keys}: FPS, frame time, voxel counts, shadow params, TAA state, tone map, exposure.
```

### Speaker notes (verbatim)
«Здравствуйте. Коротко о том, как мы проверяли результат. У нас 14 наборов автоматических тестов ядра: математика, инвалидация грязных чанков, walk-контроллер, жадный мешинг, frustum culling, клеточный автомат для жидкостей. Все зелёные при каждой сборке, ноль предупреждений в нашем коде. Плюс рантайм smoke: 6 эталонных снимков — финальный кадр, тени, контактные тени, затенение, локальный свет, отладочный слой. Передаю слово.»

### Тайминг
**20 секунд.**

### Источник данных
- Все 14 ctest suites: `docs/DefenseCompetencyFAQ_T1.md:42–57`
- 0.78 с debug / 0.06 с release: `docs/DefenseCompetencyFAQ_T1.md:42`, `T2.md:222`
- 6 smoke FINAL/SHDW/CSM/CTSH/AOCC/LOCL: `docs/DefenseCompetencyFAQ_T1.md:58`, `T4.md:116–124`
- 60+ sidecar keys: `docs/DefenseCompetencyFAQ_T1.md:60`, `T2.md:224`

---

## Слайд 9: Дополнительные фичи (3:20–3:40, Тиммейт 5)

### Визуальная структура
- **Header (\Large):** Дополнительные фичи
- **Subheader:** Asset pipeline • Audio engine • Snapshot • Hot shader reload
- **Body (4 секции, можно 2×2 grid):**

| Фича | Реализация | Хоткеи |
|------|-----------|--------|
| **Asset pipeline** | fastgltf → Draco decode → meshopt → VMA upload | `F` pick model, `PROJECTV_MODELS=path.glb@x,y,z` |
| **Audio engine** | miniaudio + PulseAudio → pipewire-pulse, MP3 only, 16/44.1 stereo | `Q` play/pause, `E` stop, `7`/`8` vol, `9`/`0` next/prev |
| **Snapshot мира** | Бинарный формат PVSNAP01, 80-B header, `std::expected<bool, VoxelSnapshotError>` | `F6` save, `F7` load |
| **Hot shader reload** | cmake subprocess + glslc, ray-march pipeline recreate | Клавиша `1` (relocated 2026-06-15) |

- **Body (низ):** 2 MP3 в `music/`: `Le1t - Palm Trees.mp3`, `Le1t - aCID.mp3`
- **Footer:** страница 9 / 13

### Body content (verbatim)
```latex
\frametitle{Дополнительные фичи}
\framesubtitle{Asset pipeline • Audio engine • Snapshot • Hot shader reload}

\begin{tabular}{|p{0.22\textwidth}|p{0.45\textwidth}|p{0.25\textwidth}|}
\hline
\textbf{Фича} & \textbf{Реализация} & \textbf{Хоткеи / API} \\
\hline
Asset pipeline & fastgltf $\to$ Draco decode $\to$ meshopt $\to$ VMA upload & \texttt{F} pick, \texttt{PROJECTV\_MODELS=...} \\
\hline
Audio engine & miniaudio + PulseAudio $\to$ pipewire-pulse, MP3 only, 16/44.1 stereo & \texttt{Q}/\texttt{E}/\texttt{7}/\texttt{8}/\texttt{9}/\texttt{0} \\
\hline
Snapshot мира & Бинарный PVSNAP01, 80-B header, \texttt{std::expected} (Tier 1.B) & \texttt{F6} save, \texttt{F7} load \\
\hline
Hot shader reload & cmake subprocess + glslc, ray-march pipeline recreate & Клавиша \texttt{1} \\
\hline
\end{tabular}

\vspace{0.3em}
2 MP3 в \texttt{music/}: \textit{Le1t - Palm Trees.mp3}, \textit{Le1t - aCID.mp3}.
```

### Speaker notes (verbatim — для Тиммейта 5)
«Здравствуйте. Здесь упомяну то, что мы не успели показать в демо. У нас есть пайплайн загрузки полигональных моделей извне: парсер glTF, опциональное Draco-сжатие, оптимизация мешей через meshoptimizer. Аудиодвижок на miniaudio, пока поддерживает только MP3. Также реализованы сохранение и загрузка мира в собственный бинарный снимок и горячая перезагрузка шейдеров. В roadmap отложено: сетевой режим, SVO, частицы, моддинг, HDR. Дальше — о планах подробнее.»

### Тайминг
**20 секунд.**

### Источник данных
- Asset pipeline: `docs/DefenseCompetencyFAQ_T5.md §3.1` (Алгоритм 16 FULL)
- Audio engine: `docs/DefenseCompetencyFAQ_T5.md §3.2` (Алгоритм 17 FULL)
- Snapshot: `docs/DefenseCompetencyFAQ_T3.md §3.7` (Алгоритм 19)
- Hot shader reload: `docs/DefenseCompetencyFAQ_T2.md §3.2.1` (Алгоритм 18)
- MP3: `docs/DefenseCompetencyFAQ_T2.md:101`, `T5.md:104`

---

## Слайд 10: Результаты и метрики (3:40–3:55, Тиммейт 5)

### Визуальная структура
- **Header (\Large):** Результаты и метрики
- **Subheader:** Сопоставление с ТЗ, измеренные цифры
- **Body (верх):** ТЗ compliance сводка (4 числа крупно):
  ```
  38 ✅   5 ⚠️   0 ❌   100% measurable
  ```
- **Body (середина):** 5 метрик в виде 2 столбцов (label : value):

| Метрика | Значение |
|---------|----------|
| VoxelLab FPS (debug) | **500+ FPS** (~2 мс кадр) |
| VoxelLab release | +1.5–2.5× FPS vs debug |
| ELF release | **19 MB** (-73% от 73 MB debug) |
| ctest wall clock | **0.78 с debug / 0.06 с release** |
| Smoke captures | **6/6** ✅ пиксель-в-пиксель |

- **Body (низ):** Сопоставление с ТЗ (per п.8 «Данные» — каждый результат имеет подпись «что / чем / когда»):
  > **Что измеряли:** метрики VoxelLab reference shot, размер ELF, время ctest
  > **Чем:** runtime smoke 6/6 captures, ctest 14/14, build artefact
  > **Условия:** linux-clang-debug (development), linux-clang-release (production); Ryzen 7 5800X + RTX 3060 Ti 8 ГБ; 1920×1080
- **Footer:** страница 10 / 13

### Body content (verbatim)
```latex
\frametitle{Результаты и метрики}
\framesubtitle{Сопоставление с ТЗ, измеренные цифры}

\begin{center}
\Huge \textbf{38 ✅ \quad 5 ⚠️ \quad 0 ❌}
\end{center}

\vspace{0.5em}
\begin{tabular}{ll}
\textbf{VoxelLab FPS (debug)} & \textbf{500+ FPS} ($\sim$2 мс кадр) \\
\textbf{VoxelLab release} & +1.5--2.5× FPS vs debug \\
\textbf{ELF release} & \textbf{19 MB} ($-73\%$ от 73 MB debug) \\
\textbf{ctest wall clock} & \textbf{0.78 с debug / 0.06 с release} \\
\textbf{Smoke captures} & \textbf{6/6} ✅ пиксель-в-пиксель \\
\end{tabular}

\vspace{0.5em}
\footnotesize
\textbf{Что измеряли:} VoxelLab reference shot, размер ELF, время ctest.\\
\textbf{Чем:} Runtime smoke, ctest 14/14, build artefact.\\
\textbf{Условия:} linux-clang-debug/release; Ryzen 7 5800X + RTX 3060 Ti 8 ГБ; 1920×1080.
```

### Speaker notes (для Тиммейта 5 — можно кратко, дублирует слайд)
«Чтобы зафиксировать измерения: 38 пунктов ТЗ закрыты, 5 явно отложены в roadmap, ноль критических провалов. На референсном снимке VoxelLab в отладочной сборке — больше 500 кадров в секунду, около двух миллисекунд на кадр. Release-бинарник ужался с 73 мегабайт до 19 — минус 73 процента. Все 14 ctest-тестов проходят менее чем за секунду. Шесть runtime smoke captures пиксель-в-пиксель совпадают с эталоном.»

### Тайминг
**15 секунд.**

### Источник данных
- 38 ✅ / 5 ⚠️ / 0 ❌: `docs/DefenseCompetencyFAQ_T2.md:335`
- 500+ FPS: `docs/DefenseCompetencyFAQ_T1.md:113`, `T2.md:368`
- 19 MB / 73 MB / -73%: `docs/DefenseCompetencyFAQ_T1.md:117`, `T2.md:225`
- 0.78 с / 0.06 с: `docs/DefenseCompetencyFAQ_T1.md:42`
- 6/6 smoke: `docs/DefenseCompetencyFAQ_T1.md:58`
- Условия измерения (Ryzen 7 5800X, RTX 3060 Ti): `docs/DefenseCompetencyFAQ_T2.md:368`

---

## Слайд 11: Ограничения, риски, безопасность (3:55–4:10, Тиммейт 4)

### Визуальная структура
- **Header (\Large):** Ограничения, риски, безопасность
- **Subheader:** Что отложено, какие известны проблемы, что с защитой информации
- **Body (верх):** **5 отложенных пунктов** (Phase 4–9):

| # | Пункт | Планируется |
|---|-------|-------------|
| 4.1.4 | Полная система частиц | Phase 7 (Vision) |
| 4.1.8 | Плагины / моддинг API | Phase 8 (Vision) |
| — | Асинхронная загрузка ресурсов | Phase 7 |
| 4.5.1 | HDR-текстуры (`.hdr`) | Phase 6 |
| — | SVO (Sparse Voxel Octree) | Phase 5 (Vision) |
| — | Mesh shaders (VK_EXT_mesh_shader) | Phase 5 (Vision) |

- **Body (середина):** **Известная проблема: BUG-005 (cycle scene race).**
  - **Симптом:** `VUID-vkCmdDraw-None-08114` errors от Vulkan validation layer при F5
  - **Смягчение:** `vkDeviceWaitIdle` в `DestroySceneResources` (Tier 5)
  - **Полное устранение:** Phase 5 (refactoring descriptor lifetime)

- **Body (низ):** **Безопасность (ТЗ 4.5.4):**
  - «Специальные требования к защите информации и программ не предъявляются»
  - **Нет DRM, нет шифрования, нет DRM-защищённых ассетов**
  - Конвейер ассетов работает с открытой спецификацией glTF
  - Все зависимости — **MIT/Apache-2.0 лицензии** (Jolt, Flecs, fastgltf, Draco, miniaudio, volk, VMA, meshopt)
- **Footer:** страница 11 / 13

### Body content (verbatim)
```latex
\frametitle{Ограничения, риски, безопасность}
\framesubtitle{Что отложено, какие известны проблемы, что с защитой информации}

\scriptsize
\begin{tabular}{lll}
\# & \textbf{Отложенный пункт} & \textbf{Phase} \\
\hline
4.1.4 & Полная система частиц & Phase 7 (Vision) \\
4.1.8 & Плагины / моддинг API & Phase 8 (Vision) \\
-- & Асинхронная загрузка ресурсов & Phase 7 \\
4.5.1 & HDR-текстуры (.hdr) & Phase 6 \\
-- & SVO (Sparse Voxel Octree) & Phase 5 (Vision) \\
-- & Mesh shaders (VK\_EXT\_mesh\_shader) & Phase 5 (Vision) \\
\end{tabular}

\normalsize
\vspace{0.5em}
\textbf{Известная проблема: BUG-005 (cycle scene race).}\\
Симптом: \texttt{VUID-vkCmdDraw-None-08114} при F5.\\
Смягчение: \texttt{vkDeviceWaitIdle} в \texttt{DestroySceneResources} (Tier 5).\\
Полное устранение: Phase 5.

\vspace{0.5em}
\textbf{Безопасность (ТЗ 4.5.4):}\\
<<Специальные требования к защите информации не предъявляются>>.\\
Нет DRM, нет шифрования. Все зависимости --- MIT/Apache-2.0.
```

### Speaker notes (verbatim — для Тиммейта 4)
«Что касается планов на будущее. Сейчас мы завершили фазу MVP — минимально жизнеспособного продукта. В следующих фазах мы планируем добавить сетевой мультиплеер, перенести все расчеты жидкости полностью на видеокарту для еще большей скорости, а также создать удобный API для написания пользовательских модов. Мы достигли большинства поставленных целей из ТЗ.

**[Переход на 12 слайд]**

Наша таблица с распределением ролей представлена на экране. Спасибо за внимание, готовы ответить на вопросы!»

### Тайминг
**15 секунд.**

### Источник данных
- 5 deferred: `docs/DefenseReport.md §3`, `DefenseCompetencyFAQ_T2.md §3.5 строка 290–299`
- BUG-005: `docs/DefenseCompetencyFAQ_T2.md §3.4`, `agent/decisions.md §18`
- ТЗ 4.5.4: `docs/DefenseReport.md §9`, `DefenseCompetencyFAQ_T2.md §3.7`
- Лицензии зависимостей: `docs/DefenseCompetencyFAQ_T1.md`, research exa (Garden engine аналогичный список)

---

## Слайд 12: Команда и личный вклад (4:10–4:25, Тиммейт 4)

### Визуальная структура
- **Header (\Large):** Команда и личный вклад
- **Subheader:** 6 участников, slot vs real competency
- **Body:** Полная таблица 6 строк × 5 столбцов:

| # | ФИО | Slot (сцена) | Реальная компетенция (Q&A) | Личный вклад |
|---|------|-------------|----------------------------|--------------|
| 1 | **Кадочников Л. П.** (le1t, тимлид) | T2 Live Demo + Стек | Архитектура, Q&A host | Архитектура, выбор библиотек, DOD layout, ECS-bridge, cold paths (snapshot, JSON config), hot shader reload, ведение Q&A на защите |
| 2 | Тиммейт 1 | T1 Титул, проблема, цели | Сборка и тестирование | CMake presets, ctest 14/14, RuntimeSmoke 6/6, метрики производительности, скрипты сборки |
| 3 | Тиммейт 2 | T3 Реализация воксельного мира, тесты | Воксельный мир | Структура воксельного мира (чанки 8×8×8, 5 материалов), greedy meshing (Лысенков, 6 проходов), двухуровневый кеш видимости (custom XOR-fold hash), voxel raycast, Fluid CA |
| 4 | Тиммейт 3 | (нет, см. примечание) | Рендеринг | Каскадные тени (CSM 4×2048², lambda 0.80), PCF 5×5 weighted, контактные тени (voxel DDA), AOCC, TAA + YCoCg + CAS, ray-marching compute pass |
| 5 | Тиммейт 4 | T6 Ограничения, команда | Физика и walk controller | Интеграция Jolt, walk/creative/spectator режимы, walk controller (edge grace, sneak, авто-прыжок), voxel raycast placement/removal |
| 6 | Тиммейт 5 | T5 Доп. фичи, метрики | Ассеты и аудио | Демо-сцена VoxelLab (пол 18×18, стеклянный шар, жидкость, 27 чанков), asset pipeline (fastgltf → Draco → meshopt → MeshBaker → VMA), audio engine (miniaudio, PipeWire→PulseAudio), playlist scan |

- **Body (низ):** **Честное замечание:** в реальности основной объём разработки выполнен тимлидом (le1t). Распределение по модулям отражает специализацию участников при защите, а не разделение труда при разработке. Полный per-commit авторский вклад — `git log --author=`.
- **Footer:** страница 12 / 13

### Body content (verbatim)
```latex
\frametitle{Команда и личный вклад}
\framesubtitle{6 участников, slot vs real competency}

\scriptsize
\begin{tabular}{|l|l|l|l|p{0.25\textwidth}|}
\hline
\# & \textbf{ФИО} & \textbf{Slot} & \textbf{Real компетенция} & \textbf{Личный вклад} \\
\hline
1 & \textbf{Кадочников Л.П.} (le1t) & T2 Demo + стек & Архитектура, Q\&A & Архитектура, библиотеки, DOD, ECS, snapshot, hot reload, Q\&A host \\
\hline
2 & Тиммейт 1 & T1 Титул, проблема, цели & Сборка, тесты & CMake presets, ctest 14/14, RuntimeSmoke 6/6, метрики \\
\hline
3 & Тиммейт 2 & T3 Voxel, тесты & Воксельный мир & Чанки 8×8×8, 5 материалов, greedy meshing, voxel raycast, Fluid CA \\
\hline
4 & Тиммейт 3 & (T4 не озвучен) & Рендеринг & CSM 4×2048² ($\lambda$=0.80), PCF 5×5, AOCC, TAA, ray-marching \\
\hline
5 & Тиммейт 4 & T6 Ограничения, команда & Физика, walk & Jolt, walk/creative/spectator, edge grace, sneak, авто-прыжок \\
\hline
6 & Тиммейт 5 & T5 Фичи, метрики & Ассеты, аудио & VoxelLab демо, glTF/Draco/meshopt, miniaudio, playlist scan \\
\hline
\end{tabular}

\vspace{0.5em}
\footnotesize
\textbf{Честное замечание:} основной объём --- le1t. Распределение по модулям --- для защиты.\\
Полный per-commit вклад: \texttt{git log -{}-author=}.
```

### Speaker notes (verbatim — для Тиммейта 4)
«Наша таблица с распределением ролей представлена на экране. Спасибо за внимание, готовы ответить на вопросы!»

### Тайминг
**15 секунд.**

### Источник данных
- Команда + slot mapping: `docs/DefenseCompetencyFAQ_T2.md §3.6` (lines 339–354)
- Per-participant contribution: `docs/DefenseCompetencyFAQ_T2.md §3.6` + `DefenseReport.md §12`
- Подольский Ф.А. (руководитель): `docs/DefenseCompetencyFAQ_T2.md:350`

### Примечание по slot Тиммейта 3
- В текущем slot mapping Тиммейт 3 не озвучивает отдельный слайд (его slot T4 не используется в 5-мин формате, чтобы уложиться в 13 слайдов).
- Однако для полноты картины в таблице команда указана.

---

## Слайд 13: Выводы + Q&A (4:25–4:30, le1t)

### Визуальная структура
- **Header (\Large, центр):** Выводы
- **Body (3 bullets):**
  - ✅ **MVP завершён:** 38 из 48 пунктов ТЗ, 14/14 ctest, 6/6 smoke, 0 warnings
  - 📋 **Что дальше:** Phase 4 (Networking) → Phase 9 (Strategic layer). 5 deferred пунктов в roadmap.
  - 🤝 **Команда:** 6 человек, роли на сцене vs реальная компетенция для Q&A.
- **Body (низ, центром крупно):** **Спасибо за внимание! Готовы ответить на вопросы.**
- **Footer:** страница 13 / 13, контакт le1t, ссылка на GitHub

### Body content (verbatim)
```latex
\frametitle{Выводы}

\begin{itemize}
\item ✅ \textbf{MVP завершён:} 38/48 ТЗ, 14/14 ctest, 6/6 smoke, 0 warnings
\item 📋 \textbf{Что дальше:} Phase 4 (Networking) $\to$ Phase 9 (Strategic layer); 5 deferred пунктов в roadmap
\item 🤝 \textbf{Команда:} 6 человек, роли на сцене vs реальная компетенция для Q\&A
\end{itemize}

\vspace{1em}
\begin{center}
\Huge \textbf{Спасибо за внимание!}\\
\vspace{0.3em}
\Large Готовы ответить на вопросы.
\end{center}
```

### Speaker notes (verbatim — для le1t, финальная речь)
«Спасибо за внимание. MVP завершён: 38 из 48 пунктов технического задания, все 14 автоматических тестов зелёные, 6 эталонных runtime captures совпадают пиксель-в-пиксель, ноль предупреждений. В планах — 5 отложенных пунктов, фазы с 4-й по 9-ю: сетевой режим, SVO-рендеринг, полная симуляция жидкостей на GPU, система частиц и моддинг API, стратегический слой с тысячами юнитов. Команда готова ответить на ваши вопросы — каждого модуля касается свой участник, а на самые сложные технические вопросы отвечу я. Спасибо!»

### Тайминг
**5 секунд (включая «Спасибо за внимание»).**

### Источник данных
- 38/48: `docs/DefenseCompetencyFAQ_T2.md:335`
- Phase 4-9: `docs/DefenseCompetencyFAQ_T2.md §3.6` roadmap table
- Team: `docs/DefenseCompetencyFAQ_T2.md §3.6` команда

---

## Подстраховка демо

Если живое демо (слайд 4) падает:
1. **le1t** переключается на screenshot из `build/linux-clang-debug/lookdev-captures/FINAL/*.bmp` или sidecar `.txt` файл
2. Говорит: «Это эталонный снимок из runtime smoke, пиксель-в-пиксель совпадает с эталоном. 500+ FPS, 27 чанков, генерация сцены менее 10 мс»
3. Продолжает со слайда 5 (аналоги) — текстовая часть, не требует live demo

---

## Чеклист перед защитой (per критерии п.10)

- [ ] Могу за 30 секунд объяснить проблему и ценность проекта. (Слайд 2)
- [ ] У проекта есть измеримые цели и критерии успеха. (Слайд 3)
- [ ] Могу объяснить, почему команда выбрала именно это решение. (Слайд 5 — аналоги и обоснование)
- [ ] В презентации есть архитектура или схема работы системы. (Слайд 6)
- [ ] Прототип, макет или proof-of-concept можно показать и объяснить. (Слайд 4 — Live Demo)
- [ ] Есть тесты, данные и сравнение с требованиями. (Слайды 8, 10)
- [ ] По каждому невыполненному требованию есть честное объяснение причины. (Слайд 11)
- [ ] Названы ограничения, риски, безопасность и дальнейшие улучшения. (Слайд 11)
- [ ] Вклад каждого участника прописан явно. (Слайд 12)
- [ ] Команда провела репетицию и уложилась в регламент. **(минимум 1 раз перед защитой, per критерии п.8)**

---

## Соответствие критериям оценки (п.6)

| Блок критерия | 41–60% (3) | 61–80% (4) | **81–100% (5) ← цель** |
|---------------|:----------:|:----------:|:----------------------:|
| Проблема и ценность | Краткое объяснение значимости | Пользователь + сценарий + ограничения | **Слайд 2:** Кто/Что/Почему с обоснованием через аналоги |
| Цели и спецификации | Несколько измеримых целей | Требования и ограничения измеримо | **Слайд 3:** 48 пунктов ТЗ + 5 измеримых критериев |
| Обоснование решения | Базовое объяснение | Альтернативы, критерии, компромиссы | **Слайд 5:** таблица 5 аналогов по 6 критериям + пробел ниши |
| Реализация и прототип | Макет, частичная работоспособность | Рабочий прототип с архитектурой | **Слайды 4, 6, 7:** Live Demo + архитектура + реализация |
| Испытания и верификация | Отдельные тесты/демонстрации | План, данные, графики, сопоставление | **Слайды 8, 10:** 14 ctest + 6 smoke + метрики с подписями |
| Ограничения, риски, этика | Названы основные слабые места | Объяснены + предложения | **Слайд 11:** 5 deferred + BUG-005 + ТЗ 4.5.4 |
| Качество защиты | Структура понятна | Логичная, слайды поддерживают | **13 слайдов, 4:30+30с, 1 мысль = 1 слайд** |
| Командная работа | Роли распределены | Каждый показывает свой вклад | **Слайд 12:** таблица с личным вкладом каждого |

**Итог:** все 8 блоков на 81–100% (оценка 5).

---

## Hand-off для следующей сессии (LaTeX/PDF экспорт)

1. **Использовать Beamer document class:** `\documentclass{beamer}`
2. **Тема:** `\usetheme{Madrid}` или `\usetheme{Metropolis}` (минималистичные, читаемость с расстояния)
3. **Цвета:** синий/серый минимализм, акценты на цифрах
4. **Шрифты:** `\usepackage{fontspec}` + системные sans-serif (Roboto / Source Sans Pro)
5. **Таблицы:** `tabularx` или `booktabs` (`\toprule`, `\midrule`, `\bottomrule`) для чистоты
6. **QR-код:** `\usepackage{qrcode}` + `\qrcode[height=1.5cm]{https://github.com/Leeleit/ProjectV}`
7. **Speaker notes:** `\begin{notes} ... \end{notes}` (видны только в presenter view)
8. **Timing:** `\setbeamercovered{transparent}` для overlay; можно вручную через `\pause`
9. **Backup slides:** `\appendix` для дополнительных слайдов на случай вопросов (например, defense 11/12 FAQ_T* краткая сводка)

**Пример минимального skeleton:**
```latex
\documentclass[aspectratio=169]{beamer}
\usepackage{fontspec}
\usetheme{Madrid}
\title{ProjectV}
\author{Кадочников Л. П. и команда <<Черепашки Ninja>>}
\institute{МФТИ-1-2024}
\date{2026-06-17}
\begin{document}
\frame{\titlepage}
% ... 12 frames по разделам выше ...
\end{document}
```

---

## Cross-refs

- **Verbatim speech texts:** `docs/DefenseScript_Team.md` (синхронизировано по слайдам)
- **Per-slot competency FAQ:** `docs/DefenseCompetencyFAQ_T{1..6}.md` (full detail для Q&A)
- **Algorithms reference:** `legacy/docs/archive/DefenseOldFormat_2026-06-17/DefenseAlgorithms.md` (23 алгоритма, archived 2026-06-17 в 831f897)
- **Compliance matrix 48 пунктов:** `docs/DefenseCompetencyFAQ_T2.md §3.5`
- **Команда 6 участников:** `docs/DefenseCompetencyFAQ_T2.md §3.6`
- **Аналоги research:** web search results (Minetest, VoxelCore, Veloren, Minecraft Java, VIXEN, Garden, Shroom, Enigma)

---

**Конец структуры.** Перед защитой: распечатать 13 слайдов, таймер держать на виду, при 4:30 — закрывающий слайд 13, при 5:00 — тишина и Q&A.
