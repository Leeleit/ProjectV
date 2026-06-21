# Sources — Ballistic Projectile Simulation

## Tier 1 (primary, directly used in prototype)

1. **NashDrilla/WarThunder-ProjectileSimulation** (2022) — C++ projectile motion with drag, RK4 integration. `github.com/NashDrilla/WarThunder-ProjectileSimulation`
2. **War Thunder DeMarre formula** — `wiki.warthunder.com/jacob_de_marre` — AP/APC/APCBC/APCR penetration calculation since 2019.
3. **Tank Archives — Penetration Equations** (2026) — DeMarre vs Krupp formula comparison with real WWII data. `tankarchives.com/2014/10/penetration-equations.html`
4. **TinyComputers.io BC5D lookup tables** (2026-01) — 5D ballistic coefficient lookup tables, piecewise-linear interpolation. `tinycomputers.io/posts/discretizing-continuous-ml-models-offline-ballistic-coefficient-corrections.html`
5. **EmpiresCommunity/ECSProjectiles** (2021) — ECS projectile system with Niagara GPU particles, 40k bullets at 16.66ms. `github.com/EmpiresCommunity/ECSProjectiles`
6. **War Thunder Datamine** — `gszabi99/War-Thunder-Datamine` — `dragCx`, penetration presets, shell configs.
7. **War Thunder volumetric shells** — `warthunder.com/en/news/6856-development-volumetric-shells` — multi-ray shell simulation since 2021.
8. **War Thunder ballistics devblog** — `warthunder.com/en/news/7310-development-dynamics-of-war-thunder-missiles-en` — missile dynamics, drag coefficient specification.
9. **War Thunder penetration mechanics** — `warthunder.com/en/devblog/current/781` — DeMarre normalization, slope effects, angle modifiers.
10. **OpenBallistics** (2026) — C++ header-only external ballistics library. `pypi.org/project/openballistics/`

## Tier 2 (supporting references)

11. **Calculator Ultra — Krupp/DeMarre calculator** — `calculatorultra.com/en/tool/armour-piercing-projectile-penetration-calculator.html`
12. **NavWeaps — Major Historical Naval Armor Penetration Formulae** — Okun resource on DeMarre/Krupp history. `navweaps.com/index_nathan/Hstfrmla.php`
13. **Combined Fleet — DeMarre/Krupp formulae** — `combinedfleet.com/formula.htm`
14. **Kbismarck.org — Krupp formula discussion** — `kbismarck.org/forum/viewtopic.php?t=43`
15. **Big Ballistics v1.2** — GeneralStaff.org ballistic evaluation tool with range table generation.
16. **helenl9098/GPU-Particle-Projectile-Customizer** (2019) — GPU particle projectile system.
17. **MidManStudio ProjectileSystem** (Unity 2026) — GPU instanced projectile rendering.
18. **danielkmb2/ballistic-kinematics** (Unity 2017) — Analytical ballistic trajectories.
19. **OpenBallistics Manual** — Drag models, Mach-dependent coefficients, environment parameters.

## Tier 3 (context)

20. **XplicitMaterials Armor Geometry Calculator** — `xplicitmaterials.com/simulation-tools/armor-time/`
21. **Unity Toolkit for Ballistics 2026** — `assetstore.unity.com/packages/tools/physics/toolkit-for-ballistics-2026-337806`
22. **blobcreate projectile-toolkit docs** — `blobcreate.github.io/projectile-toolkit/docs/Manual`
