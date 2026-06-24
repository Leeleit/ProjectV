# Sources — 2026-06-22-weather-svo-metafield

> 11 primary sources verified via direct `webfetch` to canonical URLs (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per the web_search fallback chain). Tier 1 = Wikipedia canonical reference, Tier 2 = ProjectV cross-references.
>
> Captured `2026-06-22`. All Wikipedia articles accessed via `https://en.wikipedia.org/wiki/{article}`.

---

## Tier 1: Atmospheric physics foundations (11 sources)

1. **Wikipedia: Numerical weather prediction** (Tier 1) — Lorenz 1963 chaos, Bauer 2015 "The quiet revolution of numerical weather prediction" Nature, primitive equations, parameterization, ensemble forecasting, 1-4 min regional timestep, 5-25 km grid, 14-day forecast limit. **Why important:** Direct canonical reference for NWP methodology. Establishes that 1-Hz advection update is sufficient for tactical gameplay (real NWP uses 1-4 min for regional). URL: https://en.wikipedia.org/wiki/Numerical_weather_prediction

2. **Wikipedia: Atmospheric model** (Tier 1) — Barotropic/baroclinic/hydrostatic/nonhydrostatic, regional vs global, 5-25 km typical gridboxes, 1-4 min timestep regional, "the small-scale orographic effects" problem, "fire + atmosphere" coupled models. **Why important:** Foundation for understanding that per-chunk advection is the right granularity. URL: https://en.wikipedia.org/wiki/Atmospheric_model

3. **Wikipedia: Advection** (Tier 1) — Continuity equation, semi-Lagrangian / upstream / Lax-Wendroff / MUSCL schemes, CFL condition, skew-symmetric form, 1D/2D/3D advection, "computational cost" tradeoff. **Why important:** Establishes that 1st-order upstream advection is cheap + correct for our use case (production weather NWP uses semi-Lagrangian for accuracy, but our tactical scale + 1-Hz tick + per-chunk granularity = 1st-order upstream is fine). URL: https://en.wikipedia.org/wiki/Advection

4. **Wikipedia: Coriolis force** (Tier 1) — f=2Ω sin(φ), Rossby number Ro=U/(fL), geostrophic balance, deflection to right (NH) / left (SH), inertial circles, Eötvös effect. **Why important:** Geostrophic wind is the equilibrium between pressure gradient + Coriolis — exactly the "weather front simulation" we need for E_NWPLite. Ro determines when Coriolis matters (small Ro = synoptic-scale, our 16³ = mesoscale). URL: https://en.wikipedia.org/wiki/Coriolis_force

5. **Wikipedia: Humidity** (Tier 1) — RH = p_w/p_s, absolute/specific/relative humidity, dew point, saturation pressure (Buck equation), water vapor greenhouse effect, latent heat, condensation/dew/fog. **Why important:** Per-chunk humidity field is one of 4 vars; consumer = IRST atmospheric τ, visibility fog, fluid CA precipitation, fire humidity. URL: https://en.wikipedia.org/wiki/Humidity

6. **Wikipedia: Wind** (Tier 1) — Causes (pressure gradient + Coriolis + friction), geostrophic wind, thermal wind, ageostrophic wind, log wind profile, "mass transport" patterns. **Why important:** Wind = vector field on chunk; the per-chunk wind_xz is one of 4 vars. Establishes pressure-gradient-force = primary cause. URL: https://en.wikipedia.org/wiki/Wind

7. **Wikipedia: Cellular automaton** (Tier 1) — von Neumann (4-cell cross) / Moore (8-cell) neighborhoods, Conway's Game of Life, Wolfram 4 classes, lattice gas automata, "in 1969 German computer pioneer Konrad Zuse published Calculating Space", Ising model in physics. **Why important:** D_CA_Advection strategy uses cellular automaton to advect humidity/temperature/wind between chunks. CA = proven technique for 2D/3D fluid-like simulation. URL: https://en.wikipedia.org/wiki/Cellular_automaton

8. **Wikipedia: Atmospheric pressure** (Tier 1) — 101.325 kPa standard, barometric formula, 1.2 kPa/100m at low altitude, SLP, hydrostatic, Coriolis-modified. **Why important:** E_NWPLite tracks 2D pressure field; pressure gradient force → wind. URL: https://en.wikipedia.org/wiki/Atmospheric_pressure

9. **Wikipedia: Precipitation** (Tier 1) — 4 mechanisms for cooling air to dew point (adiabatic/conductive/radiational/evaporative), Bergeron process, convective vs stratiform vs orographic rainfall, Quantitative Precipitation Forecast. **Why important:** Per-chunk humidity → rain trigger = one of 5 consumer-callback chains. RH > 0.95 → precipitation active. URL: https://en.wikipedia.org/wiki/Precipitation

10. **Wikipedia: Ideal gas law** (Tier 1) — pV=nRT, p=ρR_specific·T, R_specific for dry air = 287.058 J/(kg·K), link to air density, M_dry=0.02896968 kg/mol. **Why important:** Per-chunk air density is derived from p/T via ideal gas law. ρ = p/(R_specific·T). URL: https://en.wikipedia.org/wiki/Ideal_gas_law

11. **Wikipedia: Planetary boundary layer** (Tier 1) — 50-2000m depth, 10% surface layer, log wind profile, nocturnal/diurnal cycle, Ekman layer cross-isobar angle 10-50°, wind profile power law vs logarithmic. **Why important:** Our 8³-chunk field is 8m resolution, much coarser than PBL. We can skip PBL dynamics and use geostrophic balance (free atmosphere). URL: https://en.wikipedia.org/wiki/Planetary_boundary_layer

---

## Tier 2: ProjectV cross-references (consumer systems that need this field)

Closed experiments that benefit from `WeatherField::Query()`:
- `2026-06-21-wind-simulation-ballistics` [mixed] — per-projectile wind correction consumer
- `2026-06-21-precomputed-atmospheric-sky` [yes] — atmospheric τ for visual sky
- `2026-06-21-volumetric-fog-atmosphere-rendering` [mixed] — humidity → fog density
- `2026-06-21-cloudscape-rendering` [mixed] — humidity/temp → cloud formation
- `2026-06-21-radar-detection-system-simulation` [yes] — precipitation → radar clutter
- `2026-06-22-irst-thermal-imaging-detection` [mixed] — atmospheric τ → IRST extinction
- `2026-06-22-acoustic-detection-system` [mixed] — sound attenuation consumer
- `2026-06-21-fixed-wing-flight-model-simulation` [yes] — air density → lift/drag
- `2026-06-21-helicopter-rotor-physics` [yes] — density/icing → rotor
- `2026-06-21-ballistic-projectile-simulation` [yes] — wind drift → projectile
- `2026-06-21-wildfire-propagation` [yes] — humidity → fire spread
- `2026-06-21-fluid-ca` [yes GPU Stage 3.1] — precipitation → water cycle
- `2026-06-21-recon-intel-fog-of-war` [yes] — weather intel input

Open concepts (from `backlog.md`):
- `battlefield-weather-forecast-display` [m Tier 4] — UI consumer
- `weather-ai-modifier` [m Tier 2] — AI slows in bad weather
- `aircraft-icing-simulation` [m Tier 1] — advanced icing model

---

## Tier 3: Books / Canonical references (deferred — paywall / too long to fetch)

- Stull 1988 "An Introduction to Boundary Layer Meteorology" — canonical for PBL physics
- Holton 2004 "An Introduction to Dynamic Meteorology" — canonical for primitive equations
- Arakawa & Lamb 1977 "Computational design of the basic dynamical processes of the UCLA general circulation model" — Arakawa grids
- Lorenz 1963 "Deterministic Nonperiodic Flow" — chaos discovery
- Bauer et al. 2015 "The quiet revolution of numerical weather prediction" Nature 525, 47-55
- Warner 2010 "Numerical Weather and Climate Prediction" Cambridge University Press

These are referenced via Wikipedia citations — accepted for Tier 3 cross-references.
