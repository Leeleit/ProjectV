# Sources — 2026-06-21-aerial-perspective

## Tier 1 (primary, directly referenced)

1. **Preetham, Shirley, Smits 1999** — "A Practical Analytic Model for Daylight". SIGGRAPH 99.
   Canonical analytic sky + aerial perspective model. Turbidity-based, Rayleigh+Mie scattering.
   Flat-earth assumption. Our E strategy reference model.
   URL: https://courses.cs.duke.edu/fall01/cps124/resources/p91-preetham.pdf

2. **Hillaire 2020** — "A Scalable and Production Ready Sky and Atmosphere Rendering Technique".
   EGSR 2020. SOTA production atmosphere rendering. Aerial Perspective LUT (3D volume texture
   in camera frustum). Used in UE4/5 SkyAtmosphere. ~0.08-0.10 ms on RTX 3060-4080.
   URL: https://sebh.github.io/publications/egsr2020.pdf

3. **elliahu/atmosphere** — Complete Vulkan atmosphere renderer. Masters thesis 2025.
   Aerial perspective LUT: 0.097 ms RTX 3060, 0.080 ms RTX 4080. Validated RTX 3060/4080
   benchmarks. Cross-ref for cost model calibration.
   URL: https://github.com/elliahu/atmosphere

4. **Wenzel 2006** — "Real-time Atmospheric Effects in Games". CryEngine2 exponential
   height fog. Canonical real-time exponential fog formulation.
   URL: https://advances.realtimerendering.com/s2006/Wenzel-Real-time_Atmospheric_Effects_in_Games.pdf

5. **Google Filament** — `surface_fog.fs`. Production open-source exponential height fog
   with Beer-Lambert transmittance, sun inscattering, scene-adaptive parameters.
   URL: https://github.com/google/filament/blob/main/shaders/src/surface_fog.fs

6. **Bruneton & Neyret 2008** — "Precomputed Atmospheric Scattering". EGSR 2008.
   First real-time atmosphere from ground to space with multiple scattering. Precomputed
   LUT-based aerial perspective. ~0.01 ms at runtime.
   URL: https://inria.hal.science/inria-00288758/document

7. **Unity HDRP AtmosphericScattering.hlsl** — Production reference for Linear/Exponential/
   Volumetric fog types with sun inscattering.
   URL: https://github.com/Unity-Technologies/FPSSample

8. **Bevy PR #16314 (2025)** — Hillaire 2020 implementation merged into Bevy engine.
   Production-quality open-source atmosphere with aerial perspective.
   URL: https://github.com/bevyengine/bevy/pull/16314

## Tier 2 (supplementary)

9. **Hoffman & Preetham 2002** — "Rendering Outdoor Light Scattering in Real Time". GDC 2002.
   First GPU implementation of Preetham's model. Vertex shader aerial perspective.

10. **NVIDIA GPU Gems 3 Ch13** — "Volumetric Light Scattering as a Post-Process".
    Mitchell 2008. Aerial perspective as post-process screen-space radial blur.

11. **O'Neil 2005** — GPU-based atmospheric scattering approximation (ShaderX3).
    Simplified Nishita model for real-time use.

12. **Nishita et al. 1993** — "Display of the Earth Taking into Account Atmospheric
    Scattering". SIGGRAPH 93. Foundational atmospheric scattering paper.

13. **Morales et al. 2016** — "Real-time Rendering of Aerial Perspective Effect Based
    on Turbidity Estimation". IPSJ Transactions on Computer Vision and Applications.
    Turbidity estimation from sky images, real-time framework.

14. **Schafhitzel et al. 2007** — "Precomputed Atmospheric Scattering". 3D LUT
    parameterized by height, view angle, sun angle.

15. **DennisSmolek/SebH-TSL-Sky (2026)** — Three.js/WebGPU port of Hillaire 2020.
    Aerial-perspective haze as post-process. `policy: 'auto' | 'ap' | 'raymarch'`.
    URL: https://github.com/DennisSmolek/SebH-TSL-Sky

16. **JolifantoBambla/webgpu-sky-atmosphere (2025)** — WebGPU Hillaire 2020 implementation.
    Aerial Perspective LUT: `size: [64, 64, 32]`.
    URL: https://github.com/JolifantoBambla/webgpu-sky-atmosphere
