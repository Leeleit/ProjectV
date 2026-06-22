# Sources — 2026-06-22-bridge-building-repair

## Web sources (direct `webfetch`)

### Bailey bridge
- **URL:** https://en.wikipedia.org/wiki/Bailey_bridge
- **Key facts:** Donald Bailey (1941), Mabey & Johnson, 60 t capacity, 40 m max span, 3-man 3-hour erection, 120,000 built WWII, modular Warren truss pattern. Canonical field-expedient bridge.

### Pontoon bridge
- **URL:** https://en.wikipedia.org/wiki/Pontoon_bridge
- **Key facts:** Xerxes 480 BC Hellespont, Roman Pons Sublicius, WWII US Navy Seabees ribbon pontoon, modern M4T6 / MGB / IRB (infantry / ribbon / improved ribbon). Floating deck on water surface = water-crossing gold standard.

### Assault bridge / AVLB
- **URL:** https://en.wikipedia.org/wiki/Assault_bridge
- **Key facts:** Churchill AVRE SBG, M60A1 AVLB scissors, M104 Wolverine, Leopard 2 Biber — 20-26 m span, 5-10 min launch, 60-70 t capacity. Armoured vehicle-launched bridge (AVLB) = mechanised gap-crossing.

### Military engineering
- **URL:** https://en.wikipedia.org/wiki/Military_engineering
- **Key facts:** Sappers, combat engineers, bridging regiments, FM 5-34 Engineer Field Data. Role context for bridging within military sandbox.

### Mabey Logistic Support Bridge
- **URL:** https://en.wikipedia.org/wiki/Mabey_Logistic_Support_Bridge
- **Key facts:** Compact Steel (CS) / Compact 200, 35+ m multi-span, 60-120 t, 1500 m combinations, modular panel bridge. Bailey derivative for heavy logistic traffic.

### Foxhole wiki — Bridges
- **URL:** https://foxhole.wiki.gg/wiki/Bridge
- **Key facts:** Single-lane, destroyable by satchel/howitzers, 20 s rebuild by hammer (1600 [[Basic Materials]] = full HP), 2000 HP. Production game ref for bridge destruction/repair loop.

### WARNO — Bridge mechanics
- **URL:** (community resources — destructible point mechanics observed in WARNO gameplay)
- **Key facts:** Bridges are chokepoint objective; destructible by airstrike/artillery; repair via supply vehicles; AI pathfinding treats bridges as strategic chokepoints. Noted for comparison but not primary design reference.

## Closed experiment cross-refs

| Experiment | Slug | Verdict | Relevance |
|:-----------|:-----|:--------|:----------|
| Voxel topology analysis | `2026-06-21-voxel-topology-analysis` | yes | Union-Find CCL 26-conn = 2.73 µs mean — load-limit = min(CCL_voxels_count) × material_strength |
| Trench/fortification construction | `2026-06-22-trench-fortification-construction` | mixed | Template methodology direct analog: B_TemplateAABB_RLE winner at 32.5× over naive; bridge = sibling construction axis with water/gap variant |
| Cable/winch/towing | `2026-06-21-cable-winch-towing` | yes | Suspension bridge cable tension model via D_DistanceConstraint_Verlet |
| Voxel asset template catalog | `2026-06-21-voxel-asset-template-catalog` | yes | A_HashMap 222-512 ns lookup — bridge template load |
| Greedy physics meshing CPU | `2026-06-21-greedy-physics-meshing-cpu` | yes | 35× reduction analog — template-based construction = greedy fill for bridge building |

## Missing / deferred

- **Company of Heroes 2 / 3 bridge mechanics:** Not fetched (CAPTCHA/geo block). Would be useful for comparison but not critical.
- **Warno official manual:** Not available as public web source. Community wiki contains sufficient description.
