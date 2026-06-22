# Sources — Resource Harvesting Economy

## Foxhole (static node depletion, respawn, field types)

- Resource Field — https://foxhole.wiki.gg/wiki/Resource_Field
- Salvage Field — https://foxhole.wiki.gg/wiki/Salvage_Field
- Component Field — https://foxhole.wiki.gg/wiki/Component_Field
- Component Mine — https://foxhole.wiki.gg/wiki/Component_Mine

Key mechanics: 5 field types (Salvage/Coal/Component/Sulfur/Oil), 250/167 reserve nodes, 25-75min / 2.5-5h respawn, up to 3 Stationary Harvesters per field, Oil Fields infinite, global population affects respawn rate.

## Supreme Commander / FAForever (extractor buildings, adjacency bonus, reclaim)

- Adjacency bonus — https://supcom.fandom.com/wiki/Adjacency_bonus
- Reclaim — https://supcom.fandom.com/wiki/Reclaim
- Adjacency Bonus (FAForever) — https://wiki.faforever.com/en/Play/Learning/Adjacency-Bonus
- Adjacency bonuses strategy guide — https://supcom.standardof.net/supreme-commander/adjacency-bonuses/
- Definitive adjacency bonus guide — https://www.gamereplays.org/community/?showtopic=165702

Key mechanics: extractors placed on mass deposits, adjacency bonuses (12.5% per storage surrounded, 50% max), reclaim system (81% mass back from wreckage), tiered extractors (T1→T3).

## Minecraft (per-chunk seeded procedural ore distribution, depth gradient)

- Ore (feature) – Minecraft Wiki — https://minecraft.wiki/w/Ore_(feature)
- Best Y Level for Every Ore 1.21 — https://minecraftxray.com/guides/best-y-levels
- Minecraft 1.21 ore distribution — https://www.sportskeeda.com/minecraft/minecraft-1-21-ore-distribution

Key mechanics: triangular/uniform distribution per ore type, multiple generation passes per chunk, per-seed deterministic placement, depth gradient with distinct height bands, air-exposure culling.

## Factorio (depleting patches, infinite resources, drain, productivity)

- Mining — https://wiki.factorio.com/mining
- ResourceEntityPrototype — https://lua-api.factorio.com/1.1.110/prototypes/ResourceEntityPrototype.html

Key mechanics: resource drain (100% for burner/electric, 50% for big drills), productivity research extends effective yield, infinite-type resources with minimum yield floor, quality reduces drain further.

## Satisfactory (fixed purity tiers, infinite nodes, extractor overclocking)

- Resource Node — https://satisfactory.wiki.gg/wiki/Resource_Node
- Miner — https://satisfactory.wiki.gg/wiki/Miner
- Oil Extractor — https://satisfactory.wiki.gg/wiki/Oil_Extractor
- Crude Oil — https://satisfactory.wiki.gg/wiki/Crude_Oil

Key mechanics: purity tiers (Impure×0.5 / Normal×1.0 / Pure×2.0), infinite nodes (never deplete), extractor overclocking (250% with exponential power cost), 3 miner tiers (Mk.1-3).

## No Man's Sky (hotspot-based extractors, diminishing returns, class system)

- Hotspot — https://nomanssky.miraheze.org/wiki/Hotspot
- Mineral Extractor — https://nomanssky.miraheze.org/wiki/Mineral_Extractor
- Deep-Level Mineral Deposit — https://nomanssky.miraheze.org/wiki/Deep-Level_Mineral_Deposit

Key mechanics: hotspot class system (C/B/A/S: 40-100% density), extractor stacking with diminishing returns (Origins update), per-extractor cycle time, infinite resource potential, vertical stacking workaround.

## Dwarf Fortress (finite layered veins, % yield per tile, depth-dependent)

- Mining — https://dwarffortresswiki.org/index.php/Mining
- Vein — http://www.dwarffortresswiki.org/index.php/Vein
- Inorganic material definition token — https://www.dwarffortresswiki.org/index.php/Inorganic_material_definition_token

Key mechanics: vein/cluster/layer stone types, % yield per tile (25% layer / 33% vein / 100% cluster_small), per-Z-level generation (no cross-Z by default), material-specific frequency and inclusion rules, depth-dependent material distribution.

## Peak oil / Resource depletion (Hubbert curve, non-renewable scarcity)

- Hubbert curve — https://en.wikipedia.org/wiki/Hubbert_curve
- Hubbert peak theory — https://en.wikipedia.org/wiki/Hubbert_peak_theory
- Peak oil — https://en.wikipedia.org/wiki/Peak_oil
- Oil depletion — https://en.wikipedia.org/wiki/Depletion_of_oil

Key mechanics: symmetric bell-curve production rate, exponential rise → peak → exponential decline, logistic distribution, cumulative production at peak = half of ultimate recoverable resource.
