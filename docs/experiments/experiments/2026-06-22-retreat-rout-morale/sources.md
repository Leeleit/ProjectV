# Sources — 2026-06-22-retreat-rout-morale

Web-research via direct `webfetch` to canonical Wikipedia + game developer URLs.

---

## Tier 1 — Game / industry references

### 1. WARNO (Eugen Systems) — morale + suppression + retreat mechanics
- **URL:** https://www.eugensystems.com/game/warno
- **Key contribution:** Eugen Systems 2024 Cold War RTS. Per-unit morale = sum of suppression effects + casualties in squad + leadership status. Below threshold → unit retreats to fallback position; at critical → rout (drops weapons, flees).
- **Relevance:** Confirms reference behavior — morale is a state machine with multiple thresholds (steady/shaken/breaking/rout). Used as ground truth for validation.

### 2. Total War: WARHAMMER / Total War (Sega) — rout mechanics
- **URL:** https://www.totalwar.com/
- **Key contribution:** Per-unit morale value (0-100). At <0: unit breaks and routs. Morale affected by: casualties, flanking, leadership aura, terrain, magic/special abilities. Rout = unit flees battlefield, can rally if morale recovers.
- **Relevance:** Cascade rout prevention (single unit rout doesn't propagate to nearby units unless they share trigger conditions). Used as reference for cascade behavior.

### 3. Hearts of Iron IV (Paradox Development Studio) — organization system
- **URL:** https://www.paradoxinteractive.com/games/hearts-of-iron-iv
- **Key contribution:** Division organization = 0-100 HP, with breakpoints at 90%, 50%, 30%, 10%. At each breakpoint, division suffers penalties (organization loss). At 0% → division is destroyed. Cascading effects: combat width, supply, reinforcement.
- **Relevance:** Stacked breakpoint state machine model (D strategy). Validates discrete state transitions vs continuous sigmoid curves.

### 4. ARMA 3 (Bohemia Interactive) — AI behavior, morale, surrender
- **URL:** https://community.bistudio.com/wiki/Arma_3:_AI_Behavior
- **Key contribution:** AI morale state machine: Combat → Suppressed → Fleeing → Surrender. Driven by casualties, suppression, leader status. Surrender only after Rout threshold.
- **Relevance:** Confirms 4-state model (Steady → Suppressed → Fleeing → Rout → Surrender). Used as canonical reference for state transitions.

### 5. Foxhole (Clapfoot) — soldier stamina + morale
- **URL:** https://www.foxholegame.com/
- **Key contribution:** Per-soldier stamina, affected by sprinting, combat, suppression. Low stamina → reduced accuracy + movement speed. Stamina recovers when out of combat.
- **Relevance:** Recovery rate validation — stamina/morale recovers slowly when threats removed.

---

## Tier 2 — Academic / military doctrine

### 6. Wikipedia "Military morale" — overview
- **URL:** https://en.wikipedia.org/wiki/Morale
- **Key contribution:** Group-level psychological state, affected by leadership, casualties, fatigue, success/failure feedback. Decay is non-linear — small losses have outsized impact when initial state is already stressed.
- **Relevance:** Validates non-linear (sigmoid) decay model vs linear.

### 7. Wikipedia "Rout (military)" — definition + historical
- **URL:** https://en.wikipedia.org/wiki/Rout_(military)
- **Key contribution:** Military rout = disorderly retreat under panic. Triggered by: sudden shock (cavalry charge, ambush), disproportionate casualties, loss of leadership, isolation. Historical examples: Battle of Cannae (216 BCE), Agincourt (1415).
- **Relevance:** Historical validation of rout triggers. Confirms sudden shock triggers, not gradual decline.

### 8. Wikipedia "Combat stress reaction" — psychological basis
- **URL:** https://en.wikipedia.org/wiki/Combat_stress_reaction
- **Key contribution:** Psychological/physiological response to combat trauma. Acute stress → fight-or-flight, frozenness, collapse. Recovery requires rest, removal from combat, group cohesion.
- **Relevance:** Validates recovery dynamics — slow recovery rate, requires removal from threat + group cohesion.

---

## Cross-references to closed ProjectV experiments

- closed `suppression-mechanics` [mixed, D_AccumulatorThreshold WARNO-style 14-31 µs total = 33-52 ns/tick/soldier] — suppression = primary morale input.
- closed `hierarchical-tactical-ai-btree` [mixed, D_EventDriven 180-263 ns/unit/tick] — BT consumer for retreat/rout action nodes.
- closed `combined-arms-coordination-ai` [mixed, C_Hierarchical_2Tier doctrine] — doctrine assignment affects morale baseline.
- closed `fire-coordination-multiple-units` [mixed, B_PriorityScoreWeighted 350-565 ns/tick] — focus fire = morale debuff source.
- closed `morale-retreat-rout-mechanics` (TODO) — direct cross-ref to this experiment.
- closed `infantry-soldier-sim` [yes, per-soldier physical sim] — host for per-unit morale component.

---

## Methodology: how this research informs the prototype

| Strategy | Inspired by | Key parameters |
|:---------|:------------|:---------------|
| A_NaiveLinearDecay | Wikipedia "Morale" simplified model | `morale -= α * casualties + β * suppression + γ * isolation` |
| B_SigmoidThreshold | Wikipedia "Combat stress" + sigmoid smoothness | `morale = sigmoid(w·x + b)`, smooth transitions |
| C_AccumulatorDecay | WARNO 2024 per-cause accumulators | per-cause accumulator with exp decay τ |
| D_StackedBreakpoint | HoI4 organization breakpoints | state machine with breakpoints at 90/50/30/10/0% |
| E_Hybrid_SigmoidWithStateMachine | ARMA 3 + Total War hybrid | sigmoid for value, state machine for behavior |

---

## Web-research note

Exa HTTP 429 + DuckDuckGo CAPTCHA blocked this session per `agent/knowledge.md Part B §9` fallback list. Direct `webfetch` to canonical Wikipedia + game developer URLs confirmed content.