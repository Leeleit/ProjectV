# Session Checklist

Обязательный чеклист для каждой заметной сессии агента.

---

## Старт сессии

- [ ] Прочитать корневой `TODO.md`.
- [ ] Прочитать `agent/memory.md`.
- [ ] Прочитать `agent/status.md`.
- [ ] Прочитать `agent/decisions.md`, если задача затрагивает архитектуру, данные, память, рендер, оптимизацию или workflow.
- [ ] Проверить состояние рабочего дерева перед правками.
- [ ] Явно определить: задача относится к `mainline`, `extension` или `R&D`.

---

## Во время работы

- [ ] Не дублировать roadmap/protocol в `agent/`; хранить только delta-контекст поверх `TODO.md` и `AGENTS.md`.
- [ ] Не держать новый важный контекст только в переписке.
- [ ] Если появилась новая идея, риск или полезная подзадача, добавить её в `TODO.md`.
- [ ] Если принято решение, которое переживёт текущую задачу, записать его в `agent/decisions.md`.
- [ ] Если задача упирается в спорный инженерный выбор, свериться с `legacy/docs/philosophy`.

---

## Post-commit close-routine

Выполняется **автоматически** после успешного `git commit` (см. `AGENTS.md §8.1`).
Manual trigger — только при abort без commit или при чистке старых `open` записей.

- [ ] Обновить `TODO.md`.
- [ ] Отметить выполненные задачи.
- [ ] Обновить `agent/status.md`.
- [ ] Обновить `agent/memory.md`, если появился новый долгоживущий контекст.
- [ ] Обновить `agent/decisions.md`, если была принята новая договорённость.
- [ ] Зафиксировать, что делать следующим шагом.

**Дополнительно (auto-close специфика, per `AGENTS.md §8.1`):**

- [ ] `agent/active-sessions.md` — перенести запись из «Активные сессии» в «Закрытые сессии»,
  проставить `status: closed` + `closed-at` (ISO 8601 UTC) + `commit-hash` (SHA).
- [ ] Safety-net patch в `/tmp/before_*_<ts>.patch` — **оставить**, добавить footer
  `POST-COMMIT <sha>` (это fallback для следующей сессии, не «uncommitted work»).
- [ ] Если сработал keep-open criterion — добавить в `notes`:
  - `multi-commit-plan: <step>/<total>` (для multi-commit sub-plan), или
  - `held-open: operator-next-step` (если в последнем сообщении оператора есть next-step), или
  - `held-open: continues:<reason>` (если явно проставлен hold-open marker).
- [ ] Если commit не произошёл (gate не прошёл / build broken / scope collision) — сессия
  остаётся `open`, в `notes` фиксируется `BLOCKED: <gate>` + причина. Retry после фикса.
