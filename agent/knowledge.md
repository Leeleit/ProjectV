# Knowledge

Единый файл долговечных repo-specific фактов и действующих инженерных договорённостей.
Roadmap живёт в `TODO.md`, общий протокол — в `AGENTS.md`, текущий workspace — в `agent/workspace.md`.

**Pre-reset content (2026-06-24, ~2170 строк / 36 engineering contracts):** archived at
`legacy/docs/archive/2026-06-24-pre-reset-snapshot/knowledge.md`. Treat as historical
artifact — see WARNING header in that file. **DO NOT cite as authoritative.**

## Post-reset baseline

Этот файл пересоздан с нуля `2026-06-24` как часть reset baseline. До заполнения
новых active engineering contracts этот файл намеренно пуст.

### Что здесь должно жить (напоминание, см. `AGENTS.md` §4)

- **Engineering contracts** — действующие архитектурные договорённости, не per-session log.
  Формат: `## N. <topic>` + `### Решение:` + `### Почему:` + `### Cross-refs:`.
- **Runtime facts** — долговечные технические факты, лимиты, observed quirks конкретного
  hardware/tooling (NVIDIA RTX 3060 Ti, Vulkan 1.4.350, Clang 22.1.6, etc.).

### Чего здесь НЕ должно быть

- Per-session narrative → `agent/workspace.md`.
- Roadmap / приоритеты → `TODO.md`.
- Per-commit история → `CHANGELOG.md` + git log.
- Archived detail → `legacy/docs/archive/.../`.

### Анти-дублирование sentinel

При добавлении новой секции проверять `rg "^## " agent/knowledge.md` — не дублирует
ли новая секция уже существующий contract. Если дублирует — расширять существующую.

---

<!-- Post-reset: no contracts yet. Add per the format above as work progresses. -->