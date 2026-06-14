# ProjectV KT Documents — LaTeX Build

LaTeX-версия 4 контрольных точек (KT-2.1, KT-2.2, KT-3.1, KT-3.2) к защите 2026-06-15.

## Quick start (doom emacs)

1. **Dired на этой папке:** `SPC d d` → перейди в `docs/tex/`.
2. **Сборка всех PDF:** `M-x compile` → `make` → `Enter`. Жди ~60 секунд (первый запуск с подгрузкой шрифтов).
3. **Просмотр PDF:** `M-x pdf-view-open` → выбери нужный `.pdf`.
   (один раз: `M-x pdf-tools-install` — doom emacs ставит pdf-tools).

Альтернативно из терминала:

```sh
cd docs/tex
make            # 4 standalone PDF + 1 combined (5 файлов)
make clean      # убрать aux/log/build
```

## Сборка одной КТ

```sh
make KT-2.1_Architecture.pdf
```

## Регенерация .tex из .md

Если правил исходный `.md` в `../KT-*.md`, перегенерируй `.tex`:

```sh
./regen.sh
```

Скрипт:
- читает `../KT-*.md` + `KT-*.yaml`
- генерирует `<name>.tex` (standalone через `pandoc -s`) и `<name>-frag.tex`
  (body-only fragment, извлечённый `awk` из standalone — между `\begin{document}`
  и `\end{document}`, без `\maketitle` — для `\input{}` в `KT-Combined`)
- `KT-3.1_*.tex` проходит post-processing `sed 's/\.bmp/.png/g'`
  (исходный .md ссылается на .bmp, мы пре-конвертировали в .png через ffmpeg)
- собирает `KT-Combined.tex` (master, `\input` всех 4 фрагментов)

После `regen.sh` запусти `make` для пересборки PDF.

## Структура

```
docs/tex/
├── header.tex             # общий preamble (xelatex + fontspec + polyglossia:russian)
├── Makefile               # сборка всех PDF
├── regen.sh               # регенерация .tex из .md
├── README.md              # этот файл
├── KT-2.1.yaml            # pandoc metadata (title/author/date)
├── KT-2.1_Architecture.tex         # standalone
├── KT-2.1_Architecture-frag.tex    # фрагмент для combined
├── KT-2.1_Architecture.pdf         # build artifact
├── ... (× 4 КТ) ...
├── KT-Combined.tex         # master: input всех 4 фрагментов
├── KT-Combined.pdf         # сборник всех 4 КТ
├── screenshots/kt-3.1/    # конвертация bmp → png (6 файлов для KT-3.1)
└── .tmp/                  # latexmk aux/log (не для git)
```

## Зависимости (system)

- **xelatex** (пакеты: `fontspec`, `polyglossia`, `fancyvrb`, `xcolor`, `longtable`,
  `tabularx`, `array`, `booktabs`, `calc`, `etoolbox`, `tocloft`, `titlesec`,
  `fancyhdr`, `enumitem`, `graphicx`, `hyperref`, `microtype`, `geometry`,
  `csquotes`, `amsmath`, `amssymb`)
- **pandoc 3.6+** (для `regen.sh`)
- **ffmpeg** (для конвертации `.bmp` → `.png`, уже сделано — 6 PNG в `screenshots/kt-3.1/`)
- **latexmk** (для `make`)
- **awk** (для извлечения body из standalone, уже есть в любом дистрибутиве)

## Шрифты

`header.tex` требует:

- `Liberation Serif` (стандарт в texlive-fonts-extra, есть в большинстве Linux)
- `Liberation Sans`
- `Liberation Mono` (для кода и ASCII-арта; JetBrainsMono Nerd Font **не** подходит —
  у него нет полной кириллицы)

**Если шрифт отсутствует** — замени в `header.tex:14` (для `\setmainfont`) и
`header.tex:18` (для `\setmonofont`).

## Правка `.tex` в doom emacs

AUCTeX активируется автоматически для `.tex` файлов. Полезные команды:

| Клавиша | Действие |
|---|---|
| `C-c C-c` | Скомпилировать текущий файл (запустит `latexmk`/`xelatex`) |
| `C-c C-v` | Просмотр PDF |
| `C-c =` | Вставить окружение (`\begin{...}`) |
| `C-c C-e` | Закрыть окружение |
| `C-c C-r` | Скомпилировать регион |
| `M-.` | Перейти к определению (`\ref`, `\label`, `\input`) |
| `SPC m r` | (doom) RefTeX — список всех референсов |
| `SPC m p` | (doom) Preview — вставить/убрать LaTeX-команду |

## Doom emacs модули

Убедись, что в `~/.config/doom/init.el` включено:

```elisp
(doom
  :lang
  (latex +cdlatex)        ; AUCTeX + CDLaTeX
  (markdown +pandoc)      ; pandoc для регенерации .tex
  :tools
  pdf                     ; pdf-tools для просмотра
  )
```

## Известные ограничения

- **Unicode-символы в ASCII-арт диаграммах** (`▶` U+25B6, `◀` U+25C0, `✅` U+2705,
  `≈` U+2248) — `Liberation Mono` не имеет глифов для них. LaTeX выдаёт
  "Missing character" warnings, но **продолжает** компиляцию. Символы отображаются
  как **пустые** места. Это **не критично** — PDF всё равно создаётся. Если нужна
  подсветка — заменить шрифт на PT Mono / DejaVu Sans Mono (требует доустановки).
- **Длинные таблицы** (200+ строк в `KT-2.2`) автоматически используют `longtable` —
  могут разрываться между страницами.
- **Подсветка синтаксиса кода** отключена (`--no-highlight` в regen.sh), потому что
  pandoc-генерируемые `\FunctionTok`/`\NormalTok`/etc. не работают в `\input{}` контексте
  без переноса всех pygments-определений в header.tex. Код отображается моноширинным
  Liberation Mono без цветовой подсветки — это OK для ASCII-арт и C++ фрагментов в КТ.
- **BMP-скриншоты** сконвертированы в PNG однократно. Если в `docs/screenshots/kt-3.1/`
  появятся новые `.bmp`, перезапусти конвертацию:

  ```sh
  for f in ../screenshots/kt-3.1/*.bmp; do
    ffmpeg -y -loglevel error -i "$f" "screenshots/kt-3.1/$(basename "${f%.bmp}.png")"
  done
  ```

## Git

- `.tex`, `.yaml`, `Makefile`, `regen.sh`, `README.md`, `*.pdf`,
  `screenshots/kt-3.1/*.png` — **git tracked** (артефакты для отчёта преподавателю).
- `.tmp/` — **НЕ** git tracked. Если `make` ругается на stale files, добавь в
  `.gitignore` (или `.git/info/exclude`):

  ```
  docs/tex/.tmp/
  ```

## Содержимое PDF

| Файл | Страниц | Содержимое |
|---|---|---|
| `KT-2.1_Architecture.pdf` | 12 | TDD: System Context, 11 модулей, 5 алгоритмов, 22 раздела |
| `KT-2.2_Test_Report.pdf` | 10 | Test Report: план, ctest dashboard, 8+10 test cases, 3 bug reports |
| `KT-3.1_User_Guide.pdf` | 17 | User Guide: Admin (deploy) + User (6 screenshots, 7 how-to, 10 FAQ) |
| `KT-3.2_Final_Report.pdf` | 15 | Final Report: MVP 85%, 3 post-mortem, workflow, 7 lessons learned |
| `KT-Combined.pdf` | 64 | Сборник всех 4 (для печати) |

## История изменений

- **2026-06-14** — initial conversion (session-2026-06-13-kt-latex-r0).
  Pandoc 3.6 + xelatex + fontspec (Liberation Serif/Sans/Mono) + polyglossia:russian.
  5 PDF созданы: KT-2.1 (12 стр), KT-2.2 (10 стр), KT-3.1 (17 стр, 6 PNG),
  KT-3.2 (15 стр), KT-Combined (64 стр).
