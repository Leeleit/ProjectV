# Fluid CA: pressure-gate — SUPERSEDED

Дата: 2026-07-16 · Superseded: 2026-07-17

## Статус

**Superseded.** Pressure-gate и последующие symptom-патчи заменены контрактом
**инвариантов I1–I7** (CPU gameplay equalize).

Актуальный source of truth: `agent/knowledge.md` §8.

Кратко действующее правило: fall first; surface-only horizontal; `placeY ≤ ly`;
no tunnel through solid; accept if `placeY < ly`, or same-level with
`dst==0` / `src>=dst+2` / one-way lex on 2→1; lone cell stays; open-floor maxH→1.

GPU `fluid_ca.comp` не подменяет этот путь (default OFF).

## Историческая цель (архив)

Убрать осцилляцию одиночного Fluid на ровной опоре. Изначальный pressure-gate
(spread только при Fluid сверху) лепил пирамиды; далее column-height / anti-flicker
`src≥dst+2` ломали I5 (terraces). Не использовать как design.
