# ProjectV Tracing Guide (GPU / replay / Nsight)

Дата: `2026-07-15`

Пошаговый contract: **как** снять трассировку и метрики bottleneck’ов на Debug
Windows, без silent look-cuts и без «магии в голове». Высокоуровневый Tracy /
scene-preset overview — в [Profiling.md](Profiling.md). Этот файл — operational
playbook.

**Perf gate (AGENTS.md §2):** измеряем на `windows-clang-debug`, не Release.

---

## 0. Что выбрать

| Цель | Инструмент | Когда |
|---|---|---|
| Mean / 1% low FPS + pass ms на реальном пути | **InputReplay** + in-app GPU timestamps | Основной evidence для ceiling / A/B |
| Vulkan timeline / label totals | **Nsight Systems** (`-Tool Systems`) | Первый проход «кто ждёт кого» |
| Frame capture + replayAdjustedFps | **GraphicsCapture** (`ngfx-capture`) | Стабильный GPU frame cost без UI GpuTrace |
| Unit SOL (SM / ROP / RT Core) | **GpuTrace** (`ngfx` activity) | После Systems; на этом хосте CLI часто ломается (см. §5) |
| Kernel micro | **Nsight Compute** | После того как pass известен |
| Содержимое ресурсов | **RenderDoc** | Debug contents, не FPS |

**Рекомендуемый порядок:** InputReplay metrics → Systems → GraphicsCapture →
(опц.) GpuTrace в UI → ncu.

---

## 1. Build и look defaults

```powershell
cmake --build --preset windows-clang-debug-build --target ProjectV
$exe = "build\windows-clang-debug\bin\ProjectV.exe"
$work = Split-Path $exe -Parent
```

**Full look (не резать для цифры):**

| Рычаг | Default | A/B only |
|---|---|---|
| Shadow mask scale | `1.0` | `PROJECTV_RTX_SHADOW_MASK_SCALE=0.4` |
| Smooth specular | ON | `PROJECTV_RTX_SMOOTH_SPEC=0` |
| DDGI update period | `1` (каждый кадр) | `PROJECTV_DDGI_UPDATE_PERIOD=N` |
| Scene AA | из `runtime/scene.json` | менять только осознанно |

Перед замером **сними** leftover cut-env:

```powershell
@(
  'PROJECTV_RTX_SHADOW_MASK_SCALE','PROJECTV_RTX_SMOOTH_SPEC','PROJECTV_DDGI_UPDATE_PERIOD',
  'PROJECTV_BENCHMARK_FRAMES','PROJECTV_BENCHMARK_QUIT','PROJECTV_BENCHMARK_WARMUP_FRAMES',
  'PROJECTV_INPUT_REPLAY_AUTOPLAY','PROJECTV_INPUT_REPLAY_QUIT'
) | ForEach-Object { Remove-Item "Env:$_" -ErrorAction SilentlyContinue }
```

---

## 2. Uncapped present (иначе FPS упирается в VSync)

```powershell
$env:PROJECTV_PRESENT_MODE = 'MAILBOX'   # или IMMEDIATE
$env:PROJECTV_FULLSCREEN = '1'           # borderless desktop FS
$env:PROJECTV_ENABLE_VALIDATION = 'OFF'  # обязательно рядом с Nsight Graphics
$env:PROJECTV_SCENE_PRESET = 'VoxelLab'
$env:DISABLE_VK_LAYER_KHRONOS_validation = '1'
```

В логе должны быть:

- `[Present] preferred mode from env: MAILBOX`
- `[Window] PROJECTV_FULLSCREEN=1 → borderless...`

`PROJECTV_PRESENT_MODE` читается **при первом create swapchain** — менять mid-run
бесполезно без recreate.

---

## 3. In-app GPU timestamps (что означают цифры)

Query pool на кадр (10 слотов → 5 пар) в `RendererDrawFrame.cpp`:

| Log field | Pass |
|---|---|
| `gpu_tlas` | TLAS build/update |
| `gpu_rtx` | RT shadow mask dispatch |
| `gpu_ddgi` | DDGI probe update |
| `gpu_opaque` | meshing + opaque/transparent/sky/cloud до PostFX/AA |
| `gpu_aa` | PostFX + AA resolve + blit |
| `gpu_gfx` | `gpu_opaque + gpu_aa` |

Пишутся в InputReplay metrics и живут в `RenderPassTimings`.

**Важно:** `BenchmarkAutomation` `mean_ms` / insane `fps_now` часто **CPU-side**
и не равны wall GPU. Для ceiling используй InputReplay `[summary]` + `gpu_*`.

---

## 4. InputReplay tracing (primary path)

### 4.1 Запись

В игре: `F5` (record) … сценарий … `F5` stop.
Файлы по умолчанию:

- `%TEMP%\ProjectV\InputReplay\latest.projectv.replay`
- snapshot рядом (`.snapshot.bin`)

Формат **v1 only** (один sample на sim tick @ 60 Hz). Старый wall-clock
playback — удалён.

### 4.2 Автопрогон

```powershell
$replay = "$env:TEMP\ProjectV\InputReplay\latest.projectv.replay"
Test-Path -LiteralPath $replay   # must be True

$env:PROJECTV_INPUT_REPLAY_AUTOPLAY = '1'   # или полный путь к .replay
$env:PROJECTV_INPUT_REPLAY_QUIT = '1'
# + MAILBOX / FULLSCREEN / VALIDATION=OFF из §2

$base = "build\windows-clang-debug\profiler-captures\<label>"
New-Item -ItemType Directory -Path $base -Force | Out-Null
$proc = Start-Process -FilePath $exe -WorkingDirectory $work -NoNewWindow -PassThru `
  -RedirectStandardOutput "$base\out.log" `
  -RedirectStandardError "$base\err.log"
$proc.WaitForExit(600000)
```

**AUTOPLAY contract:**

- `=1` / `true` / `yes` / `on` → грузит `PROJECTV_INPUT_REPLAY_PATH` или
  `latest.projectv.replay`
- **или** путь к файлу (`C:\...\foo.projectv.replay`) — тоже включает autoplay
- `0` / `false` / `off` / unset → без autoplay

Раньше путь в `AUTOPLAY` молча игнорировался (truthy-only parser) — движок
стартовал без replay. В логе **обязаны** быть:

```
[ProjectV][InputReplay] AUTOPLAY armed replay=... frames=N quit=true
[ProjectV][InputReplay] playback started ...
```

Если их нет — autoplay не включился.

### 4.3 Что читать в логе

Периодически (границы + каждые 120 frames):

```
[InputReplay][metrics] frame=i/N dt_ms=... fps=...
  shadow_ms=... mesh_ms=... gfx_ms=... aa_ms=... post_ms=... present_ms=... other_ms=...
  gpu_tlas=... gpu_rtx=... gpu_ddgi=... gpu_opaque=... gpu_aa=... gpu_gfx=...
  dirtyRebuild=... tris=... nonair=...
```

В конце:

```
[InputReplay][summary] samples=... mean_dt_ms=... mean_fps=...
  p1_low_dt_ms=... p1_low_fps=... min_dt_ms=... max_dt_ms=...
[InputReplay] playback finished
```

`p1_low_*`: после drop первых ~10% samples (cold RTX join / hitch).

SDL_Log обычно в **stderr** → смотри `err.log`, не только `out.log`.

### 4.4 Пример разбора (full look, 2026-07-15)

На VoxelLab FS+Mailbox, mask 1.0, smooth-spec ON, DDGI period 1, scene AA
MSAA4+SMAA+1.50 (2880×1620):

| Pass | типично | вывод |
|---|---:|---|
| `gpu_opaque` | ~2–8 ms | voxels + smooth specular |
| `gpu_rtx` | ~2–5 ms | full-res shadow mask |
| `gpu_aa` | ~0.4 ms | **не** ceiling |
| `gpu_ddgi` / `gpu_tlas` | ~0.06 / ~0 | низкий приоритет |

Артефакты: `build/windows-clang-debug/profiler-captures/full-look-baseline/`.

---

## 5. Windows harness (`Invoke-ProjectVProfile.ps1`)

```powershell
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool Systems -Smoke
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool Systems -Frames 180 -Warmup 40
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool GraphicsCapture -CaptureFrame 90
.\tools\windows\Invoke-ProjectVProfile.ps1 -Tool GpuTrace -StartAfterFrames 80 -LimitFrames 2
```

Выход: `build/windows-clang-debug/profiler-captures/<Label>/`

- `summary.json` / `SUMMARY.md`
- tool artifacts (`.nsys-rep`, `.ngfx-capture`, CSV, …)

Discovery: `Resolve-ProjectVProfilerTools.ps1`
Overrides: `PROJECTV_NSYS`, `PROJECTV_NCU`, `PROJECTV_NGFX`, `PROJECTV_NGFX_CAPTURE`, …

### 5.1 Nsight Systems

```powershell
# One-time Admin — full Vulkan GPU workload in nsys
.\tools\windows\Register-ProjectVNsightVulkanLayer.ps1
```

Без Admin harness падает на `--trace=nvtx` и пишет note в `summary.json`.

Harness **не** всегда пробрасывает `MAILBOX`/`FULLSCREEN` в child — для uncapped
FPS лучше выставить env в **текущей** shell **до** вызова скрипта (скрипт
наследует process env).

`app.log` у Systems часто = stdout **nsys**, не ProjectV. Ищи
`BenchmarkAutomation` / metrics в stderr ProjectV или гоняй direct exe (§4).

### 5.2 GraphicsCapture (`ngfx-capture`)

Рабочий CLI path. Validation OFF обязателен (иначе AV в
`nvoglv64` + validation + interception).

Смотри `ngfx-perf/**/iteration_times.csv`:

- `replayAdjustedFps` — ближе к apples-to-apples
- `msGpuTime` может быть `-1` если не собрали

### 5.3 GpuTrace (`ngfx.exe --activity "GPU Trace Profiler"`)

**Известные ловушки (этот хост, 2026-07-15):**

1. **Qt platform plugin**
   Dialog: *«no Qt platform plugin could be initialized… Available: windows»*.
   Nsight кладёт плагины в `…\host\windows-desktop-nomad-x64\Plugins\` (capital P).
   Harness ставит `QT_PLUGIN_PATH` / `QT_QPA_PLATFORM_PLUGIN_PATH` и cwd = ngfx host,
   PATH без Cursor Qt.

2. **`ngfx.exe --activity …` → crash `0xC0000409`**
   Даже с чистым PATH. `ngfx --help` / `ngfx-capture --help` ок; старт **activity**
   падает. Fallback: GraphicsCapture + in-app `gpu_*`, либо GpuTrace **из UI**
   Nsight Graphics вручную.

3. **Start-Process `-ArgumentList`** режет `"GPU Trace Profiler"` по пробелам —
   harness использует bat + quoted args / call-operator.

Если `summary.json` пишет `exit=-1073740791` / note про Qt — **не** трактуй как
PASS по GPU units.

### 5.4 Compute / RenderDoc

- `ncu`: высокий overhead, маленький `Frames`.
- RenderDoc: inject + **F12** mid-run (in-app TriggerCapture ещё нет).

---

## 6. Чеклист «честного» full-look прогона

1. Debug binary свежий (`windows-clang-debug-build`).
2. Cut-env снят (§1).
3. `MAILBOX` + `FULLSCREEN=1` + `VALIDATION=OFF`.
4. Replay существует; `AUTOPLAY=1` (или path); в логе `armed` + `playback started`.
5. Дождись `[summary]` + `playback finished`.
6. Сравни `gpu_opaque` / `gpu_rtx` / `gpu_aa` и `mean_fps` / `p1_low_fps` с предыдущим
   capture в `profiler-captures/`.
7. Не объявляй DoD по цифрам, полученным с `MASK_SCALE=0.4` / smooth-spec OFF /
   DDGI period 4 без явного согласия на look tradeoff.

---

## 7. Env cheat-sheet

| Env | Назначение |
|---|---|
| `PROJECTV_SCENE_PRESET` | VoxelLab / FlatBenchmark / … |
| `PROJECTV_PRESENT_MODE` | FIFO / MAILBOX / IMMEDIATE |
| `PROJECTV_FULLSCREEN` | `1` = borderless FS |
| `PROJECTV_ENABLE_VALIDATION` | `OFF` рядом с Nsight Graphics |
| `PROJECTV_INPUT_REPLAY_AUTOPLAY` | `1` или path к `.replay` |
| `PROJECTV_INPUT_REPLAY_QUIT` | `1` = exit после playback |
| `PROJECTV_INPUT_REPLAY_PATH` / `_DIR` | override replay location |
| `PROJECTV_BENCHMARK_FRAMES` / `_WARMUP_FRAMES` / `_QUIT` | short synthetic loop (не замена replay) |
| `PROJECTV_RTX_SHADOW_MASK_SCALE` | default 1.0; lower = A/B only |
| `PROJECTV_RTX_SMOOTH_SPEC` | default ON; `=0` A/B |
| `PROJECTV_DDGI_UPDATE_PERIOD` | default 1; N>1 A/B amortization |
| `PROJECTV_RTX_SHADOW_PASS` | `0` = skip shadow pass (A/B) |

---

## 8. Связанные документы

- [Profiling.md](Profiling.md) — Tracy plots, scene presets, methodology
- [Debugging.md](Debugging.md) — hotkeys / validation
- [BuildAndRun.md](BuildAndRun.md) — Windows build
- `tools/windows/Invoke-ProjectVProfile.ps1` — harness source
- `agent/knowledge.md` — InputReplay + present-mode + AA-no-TAA contracts
