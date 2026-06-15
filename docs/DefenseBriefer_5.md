# DefenseBriefer_5.md — Памятка Тиммейта 5: Демо VoxelLab, ассеты, аудио

**Участник:** [Имя Тимейта 5]
**Слот:** 8:30–10:00 (1:30 минуты, последний перед закрытием le1t)
**Что покрываю:** демо-сцена Voxel Laboratory (что видим на экране), конвейер загрузки ассетов (glTF, Draco, meshopt), аудио-система (miniaudio), hot shader reload (F5)
**Что НЕ покрываю:** архитектура выбора (le1t), стек/билд (Тиммейт 1), voxel-мир (Тиммейт 2), рендеринг (Тиммейт 3), физика (Тиммейт 4)

---

## 1. Шапка выступления

> «Добрый день, меня зовут **[Имя Тимейта 5]**, я расскажу про демо-сцену Voxel Laboratory, конвейер загрузки ассетов и аудио-систему.»

---

## 2. Что говорить verbatim (1:30, ~220 слов)

> «Сейчас на экране вы видите нашу эталонную демо-сцену — Voxel Laboratory. Это пол в виде шахматной доски 18 на 18 вокселей с чередованием белого и светло-серого цветов. В центре стоит стеклянный шар радиуса 5 вокселей, внутри которого находится жидкость. Справа — непрозрачный якорь из тех же материалов, он нужен для стабильных теней от солнца.
>
> Сцена процедурно генерируется при старте за 200 миллисекунд. Это 27 чанков, 13 824 вокселя, индексированных плоским массивом. Пол на Y=0, центр сферы в точке (0, 8, 0). Все размеры — в воксельных координатах, шаг сетки один метр.
>
> Конвейер загрузки ассетов устроен так. Парсер fastgltf читает glTF и glb-файлы, при наличии сжатия Draco подключается Draco-декомпрессор, потом meshopt оптимизирует меш для GPU: vertex cache, overdraw, vertex fetch. Финальный bake загружает данные в видеокарту через VMA-аллокатор. Загрузчик синхронный, для 100 мегабайт glb работает меньше секунды.
>
> Аудио построено на библиотеке miniaudio — это header-only MIT-библиотека. На Linux бэкенд идёт через PipeWire с маршрутизацией в PulseAudio. Плейлист сканируется каждые пять секунд, новые файлы подхватываются автоматически. Управление музыкой — клавиши Q play/pause, E stop, 7 и 8 громкость, 9 и 0 переключение треков. Сейчас в нашей папке два трека.
>
> И ещё одна полезная возможность — горячая перезагрузка шейдеров по клавише F5. Она перекомпилирует все шейдеры и пересоздаёт конвейер без перезапуска приложения, удобно для итераций. Передаю слово ведущему для заключительной части.»

---

## 3. Понятия (14 терминов, чтобы понимать что говоришь)

| Термин | Что это в одном предложении |
|---|---|
| **VoxelLab / Voxel Laboratory** | Эталонная демо-сцена: пол 18×18, стеклянный шар, жидкость, 27 чанков. |
| **glTF / glb** | Формат 3D-моделей от Khronos. glTF = JSON, glb = binary. |
| **fastgltf** | C++ парсер glTF, header-only, MIT. |
| **Draco** | Google библиотека для сжатия 3D-мешей. Декомпрессия в runtime. |
| **meshopt** | Библиотека оптимизации мешей: vertex cache, overdraw, vertex fetch. |
| **VMA (VulkanMemoryAllocator)** | Аллокатор GPU-памяти от AMD. |
| **Manifest** | Список моделей для загрузки. Задаётся env var `PROJECTV_MODELS`. |
| **Snap above ground** | Авто-позиционирование модели над ground (Y=0). |
| **miniaudio** | Header-only аудио-библиотека, MIT. |
| **PipeWire → PulseAudio** | Linux audio маршрутизация. PulseAudio — старый бэкенд, PipeWire — новый. |
| **Playlist scan** | Авто-обновление плейлиста каждые 5 секунд. |
| **Hot shader reload (F5)** | `cmake --build --target Shaders` + pipeline recreate, без перезапуска. |
| **sidecar metadata** | `.txt` файл рядом с `.bmp` захватом, 60+ ключей метаданных. |
| **BMP capture** | Look-dev захват. Клавиша C в рантайме, sidecar в build/<preset>/lookdev-captures/. |

---

## 4. Что показывать на экране (если попросят)

**Демо 1 — стартовая сцена (10 секунд):**
- VoxelLab уже на экране.
- «Вот эта сцена. 27 чанков, 13 824 вокселя, 200 мс генерация. Стеклянный шар, жидкость внутри, пол-шахматка.»

**Демо 2 — захват (15 секунд):**
- Клавиша `C` — сохранить .bmp + .txt sidecar.
- Открыть sidecar в текстовом редакторе: показать ключи `frame_time_ms`, `scene_preset`, `voxel_count`, `chunk_count`, `shadow_cascade_*`, `taa_*`, `render_pass_*_ms`, `music_*`.
- «Видите 60+ ключей metadata. Это и есть воспроизводимость — мы можем перезапустить и сравнить capture-to-capture.»

**Демо 3 — cycle views (10 секунд):**
- Клавиша `B` — переключение отладочных видов.
- «FINAL, SHDW, CSM, CTSH, AOCC, LOCL — все debug views. На каждом видно отдельный слой рендеринга.»

**Демо 4 — audio (15 секунд):**
- Клавиша `Q` — play/pause.
- `7`/`8` — volume down/up.
- `9`/`0` — next/prev track.
- `E` — stop.
- «miniaudio, MP3 плейлист. Сейчас два трека в папке music/. Playlist сканируется каждые 5 секунд, новые подхватываются автоматически.»

**Демо 5 — hot shader reload (опционально, 30 секунд):**
- `F5` — перекомпилировать шейдеры.
- «Это hot reload — без перезапуска приложения. Полезно для итераций над шейдерами.»

---

## 5. Out of scope — куда отправлять вопросы

| Вопрос про… | Говори |
|---|---|
| Почему fastgltf, а не tinygltf | «Архитектурное решение le1t» |
| Почему miniaudio, а не OpenAL/SDL_mixer | «Архитектурное решение le1t» |
| PipeWire vs PulseAudio подробно | «Linux audio, le1t или к Тиммейту 1 (билд)» |
| Тени / TAA / AOCC | «К Тиммейту 3» |
| Voxel-мир / чанки | «К Тиммейту 2» |
| Walk controller | «К Тиммейту 4» |
| Build / ctest | «К Тиммейту 1» |
| BUG-004 / BUG-005 | «le1t расскажет» |
| JSON scene config | «le1t» |
| Snapshot save/load | «le1t» |

---

## 6. Если попросят «расскажите подробнее» (что вы можете раскрыть)

### Если спрашивают «что такое Voxel Laboratory»:
> «Это наша reference-сцена. Пол — шахматная доска 18×18 вокселей, чередующиеся белый и серый. В центре — стеклянный шар радиуса 5 вокселей, внутри жидкость. Справа — непрозрачный якорь для стабильных shadow casters. Генерируется процедурно за 200 мс. Используется для всех эталонных измерений: ctest, smoke 6/6 captures, FPS measurement (110-130 FPS, 7-9 мс debug).»

### Если спрашивают «почему именно шахматка 18×18»:
> «18×18 = 324 вокселя, чуть меньше одного чанка в каждом измерении (чанк 8×8 = 64 вокселя, 3×3 = 9 чанков на пол = 27 чанков всего). Достаточно плотно, чтобы видеть эффекты (тени, AOCC, TAA), но не настолько много, чтобы FPS падал. Reference shot для всех измерений.»

### Если спрашивают «как работает конвейер ассетов»:
> «fastgltf парсит glTF/glb → если есть Draco compression → DracoMeshDecoder декодирует → meshopt оптимизирует (vertex cache, overdraw, vertex fetch) → MeshBaker объединяет в один меш с общим material atlas → VMA загружает в GPU. Manifest через env var `PROJECTV_MODELS=path.glb@x,y,z;...`. `SnapModelInstancesAboveGroundDispatch` позиционирует модели над ground.»

### Если спрашивают «что такое Draco»:
> «Draco — это open-source библиотека от Google для сжатия 3D-мешей. Уменьшает размер glTF в 5-10 раз, за счёт этого быстрее загрузка. Цена: runtime декомпрессия перед оптимизацией. У нас DracoMeshDecoder делает это в одном hot pass.»

### Если спрашивают «что такое meshopt»:
> «meshopt — это библиотека от Артура Дашдаева (Facebook/Meta) для оптимизации мешей. Три основные операции: vertex cache optimization (реордеринг индексов для cache locality), overdraw optimization (реордеринг треугольников для уменьшения overdraw), vertex fetch optimization (реордеринг вершин для locality). Все три — preprocessing, runtime cost = 0.»

### Если спрашивают «как работает miniaudio»:
> «miniaudio — header-only аудио-библиотека, MIT. Поддерживает кучу бэкендов: WASAPI на Windows, PulseAudio/PipeWire/ALSA на Linux, CoreAudio на macOS. У нас на Linux идёт через PipeWire → PulseAudio маршрутизацию. Не требует linking, не требует отдельной сборки. AudioEngine обёртка в `src/audio/`, scanPlaylist() каждые 5 секунд.»

### Если спрашивают «что такое sidecar metadata»:
> «При захвате .bmp сохраняется .txt sidecar с метаданными. 60+ ключей: `frame_time_ms`, `scene_preset`, `voxel_count`, `chunk_count`, `shadow_cascade_*` (4 каскада: view ranges, ortho extents, texel size), `taa_*` (blend, jitter scale, history valid), `render_pass_*_ms` (per-pass timings), `music_*` (track, state, volume), `exposure_*`, `tone_map_*`, и т.д. Это и есть воспроизводимость — можно перезапустить и сравнить.»

### Если спрашивают «что такое hot shader reload»:
> «F5 в `main.cpp::SDL_AppEvent` вызывает `RebuildAllShadersFromDisk()`, который запускает subprocess `cmake --build build/<preset> --target Shaders`. glslc/glslangValidator перекомпилирует все .vert/.frag/.comp → .spv. На success → `RequestRayMarchPipelineRecreate()`. На следующем кадре pipeline recreate. Без перезапуска приложения, удобно для итераций.»

### Если спрашивают «какие треки в плейлисте»:
> «Сейчас в `music/` два трека: `Le1t - Palm Trees.mp3` и `Le1t - aCID.mp3`. Можно добавлять новые — плейлист пересканируется через 5 секунд. Поддерживаются MP3, WAV, FLAC.»

---

## 7. Cheat-card для печати (1 страница A4)

```
┌────────────────────────────────────────────────────────────────────────┐
│       BRIEFER 5 — Демо VoxelLab + ассеты + аудио (1:30)              │
├────────────────────────────────────────────────────────────────────────┤
│ НАЧАЛО: "Добрый день, меня зовут [Имя Тимейта 5], я расскажу про      │
│          демо-сцену Voxel Laboratory, конвейер загрузки ассетов        │
│          и аудио-систему."                                             │
├────────────────────────────────────────────────────────────────────────┤
│ КЛЮЧЕВЫЕ ФАКТЫ:                                                        │
│  • VoxelLab: 18×18 пол, стеклянный шар r=5, жидкость, 27 чанков        │
│  • 13 824 вокселя, генерация 200 мс                                    │
│  • Asset pipeline: fastgltf → Draco → meshopt → MeshBaker → VMA        │
│  • Manifest: PROJECTV_MODELS=path.glb@x,y,z;...                       │
│  • miniaudio: header-only MIT, PipeWire → PulseAudio                    │
│  • Playlist scan: каждые 5 секунд, MP3/WAV/FLAC                       │
│  • Audio hotkeys: Q play/pause, E stop, 7/8 vol, 9/0 next/prev         │
│  • Hot reload: F5 → cmake --target Shaders → recreate                 │
│  • Sidecar: .txt рядом с .bmp, 60+ ключей                              │
├────────────────────────────────────────────────────────────────────────┤
│ 3 ЧАСТИ ВЫСТУПЛЕНИЯ:                                                   │
│  1. VoxelLab: что видим на экране                                      │
│  2. Asset pipeline: glTF → Draco → meshopt → VMA                        │
│  3. Audio: miniaudio, плейлист, hotkeys                                │
├────────────────────────────────────────────────────────────────────────┤
│ OUT OF SCOPE → le1t: выбор библиотек, JSON config, snapshot, hotkeys  │
│              → T1: билд, метрики                                        │
│              → T2: voxel-мир                                            │
│              → T3: рендеринг                                            │
│              → T4: физика                                               │
├────────────────────────────────────────────────────────────────────────┤
│ ЕСЛИ СПРОСЯТ ГЛУБЖЕ:                                                   │
│  • VoxelLab: пол 18×18, шар r=5, 27 чанков, 200 мс генерация          │
│  • 18×18: 324 вокселя, 9 чанков на пол, reference shot                 │
│  • Asset: fastgltf → Draco → meshopt → bake → VMA upload              │
│  • Draco: Google, 5-10× сжатие, runtime decode                          │
│  • meshopt: vertex cache, overdraw, vertex fetch opt                   │
│  • miniaudio: header-only, PipeWire → PulseAudio                       │
│  • Sidecar: 60+ ключей, воспроизводимость capture-to-capture           │
│  • Hot reload: F5, cmake --target Shaders, no restart                  │
│  • Tracks: Le1t - Palm Trees, Le1t - aCID (2 MP3)                      │
└────────────────────────────────────────────────────────────────────────┘
```

---

**Конец памятки.** Перед защитой: прочитать §2 вслух 3 раза с таймером, уложиться в 1:30 ± 5 секунд. Cheat-card [§7] распечатать. После твоего выступления — передаёшь слово le1t для заключительной части и Q&A.
