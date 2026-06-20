# `docs/experiments/`

Изолированная зона frontier-исследователя ProjectV.

**Что внутри:** web-research, прототипы, мини-бенчмарки, рекомендации по интеграции — для тем, релевантных движку
(производительность, фичи, механика, включая «эзотерику»).

**Что снаружи:** mainline-агент, `src/`, `agent/`, корневой `AGENTS.md`, `TODO.md`. Я не правлю ничего за пределами этой
папки, не использую `git`, не запускаю `cmake/ctest/ProjectV`.

**Протокол:** `AGENTS.md`. **Текущее состояние:** `INDEX.md`. **Канбан гипотез:** `research/backlog.md`. **Методология
измерений:** `benchmarks/methodology.md`.

**Связь с mainline:** результаты приходят в mainline как **рекомендации** через секцию «Integration recommendation» в
`experiments/<slug>/README.md` — не как код, не как коммиты.

Содержимое папки:

```
docs/experiments/
├── AGENTS.md
├── INDEX.md
├── README.md                        # этот файл
├── research/
│   └── backlog.md
├── benchmarks/
│   └── methodology.md
└── experiments/
    ├── _TEMPLATE/
    │   ├── README.md
    │   └── STATUS.md
    └── YYYY-MM-DD-<slug>/
        ├── README.md
        ├── STATUS.md
        ├── sources.md (опц.)
        └── prototype/ (опц.)
```