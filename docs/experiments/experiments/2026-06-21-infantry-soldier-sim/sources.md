# Sources — 2026-06-21-infantry-soldier-sim

Verified design and technical references for simulating infantry state machines, weight fatigue, and limb-based medical systems.

## 1. Primary Design References (Game Systems)

### 1.1 Arma 3 Stamina & Fatigue System (Bohemia Interactive)
- **Source:** Bohemia Interactive Community Wiki (`community.bistudio.com/wiki/Arma_3:_Stamina`)
- **Key Concepts:**
  - **Stamina Pool:** A linear pool representing short-term sprint capacity (0 to 100).
  - **Fatigue:** Long-term physiological stress that directly translates to muscle fatigue and causes weapon sway.
  - **Loadout Weight Impact:** Gear weight (measured in mass units or kilograms) acts as a multiplier on stamina consumption. If loadout exceeds ~30 kg, stamina drains even during normal jogging, and recovery is heavily penalized.
  - **Heart Rate Integration:** Sprinting or low stamina causes heart rate spikes, directly increasing the magnitude and speed of weapon sway.

### 1.2 Escape from Tarkov Medical & Damage System (Battlestate Games)
- **Source:** Unofficial Escape from Tarkov Wiki (`escapefromtarkov.fandom.com/wiki/Health`)
- **Key Concepts:**
  - **Limb-Based HP Pools:** Body segmented into 7 parts: Head (35 HP), Thorax (85 HP), Stomach (70 HP), Left/Right Arm (60 HP), Left/Right Leg (65 HP).
  - **Blackout States:** A limb reaching 0 HP is "blacked out". Head or Thorax blacking out from a direct hit results in instant death.
  - **Damage Distribution:** Damage dealt to a blacked-out limb spreads to all other remaining healthy limbs based on a compartment multiplier: Stomach (1.5×), Legs (1.0×), Arms (0.7×).
  - **Status Effects:**
    - *Light Bleeding:* Slow continuous HP loss across all limbs.
    - *Heavy Bleeding:* Rapid HP loss across all limbs, leaving blood trails.
    - *Fractures:* Broken bones in legs disable sprinting; broken bones in arms increase weapon sway.

## 2. Technical References (ECS and Simulation)

### 2.1 Flecs ECS Architecture (Sander Mertens)
- **Source:** Flecs documentation (`github.com/SanderMertens/flecs`)
- **Key Concepts:**
  - Structure-of-Arrays (SoA) layout optimizes cache locality for high-density entity iteration.
  - Storing state, stamina, and limb health in flat component tables allows CPU-SIMD auto-vectorization and minimizes memory footprint (<100 bytes per soldier).
