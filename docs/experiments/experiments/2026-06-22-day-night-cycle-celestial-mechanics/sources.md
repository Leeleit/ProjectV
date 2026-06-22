# Sources — day-night-cycle-celestial-mechanics

## Tier 1 (primary)

1. **Minecraft Wiki: Day-night cycle** — `minecraft.wiki/w/Day-night_cycle`. 20 min cycle, celestial angle formula, moon phases, ambient light mechanics.
2. **Minecraft Wiki: Light** — `minecraft.wiki/w/Light`. Block light vs sky light, light level table.
3. **Wikipedia: Sunrise equation** — `en.wikipedia.org/wiki/Sunrise_equation`. Solar zenith angle computation.
4. **Wikipedia: Twilight** — `en.wikipedia.org/wiki/Twilight`. Civil/nautical/astronomical definitions.
5. **Wikipedia: Kepler's laws of planetary motion** — `en.wikipedia.org/wiki/Kepler's_laws_of_planetary_motion`. Orbital mechanics foundation.
6. **Nishita 1993 "Display of the Earth Taking into Account Atmospheric Scattering"** — canonical atmospheric scattering for twilight color.
7. **Precomputed Atmospheric Sky (closed experiment)** — `experiments/2026-06-21-precomputed-atmospheric-sky/`. C_Hillaire2020 sky LUT at 0.080 ms.
8. **Hoffman & Preetham 2002 "A Practical Analytic Model for Daylight"** — SIGGRAPH 2002, canonical analytic sky model used in games.

## Tier 2 (secondary/cross-ref)

9. **Dynamic Entity Lighting (closed experiment)** — `experiments/2026-06-21-dynamic-entity-lighting/`. Entity-as-light-source blending with ambient.
10. **War Thunder Dagor Engine day/night cycle** — GDC 2019 talk, prebaked sky cubemap keyframes.
11. **Wikipedia: Rayleigh scattering** — `en.wikipedia.org/wiki/Rayleigh_scattering`. θ⁴ scattering for sky color.
12. **Wikipedia: Mie scattering** — `en.wikipedia.org/wiki/Mie_scattering`. Aerosol scattering for horizon glow.
13. **Hipparcos star catalog** — `heasarc.gsfc.nasa.gov/docs/hipparcos/`. ~118,000 stars with RA/dec/magnitude. Basis for D star field.

## Tier 3 (supplementary)

14. **Wikipedia: Milankovitch cycles** — long-term orbital variation, informs orbital element design.
15. **Precomputed Atmospheric Sky (closed experiment) RESULTS** — 0.080 ms Hillaire2020 LUT per 30 Hz budget.
