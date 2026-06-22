# Sources — 2026-06-22-procedural-voxel-material-audio

## Tier 1 (foundational — physically-based audio synthesis)

1. **van den Doel, K., Kry, P.G., Pai, D.K. — "FoleyAutomatic: Physically-Based Sound Effects for Interactive Simulation and Animation" (SIGGRAPH 2001)**
   — <https://www.cs.mcgill.ca/~kry/pubs/foleyautomatic/foleyautomatic.pdf>
   Canonical paper on real-time contact sound synthesis using modal models driven by contact forces from dynamic simulation. Impact, rolling, sliding sounds. Core reference for Strategy C (modal synthesis).

2. **Cook, P.R. — "Physically Informed Sonic Modeling (PhISM): Percussive Synthesis" (ICMC 1996)**
   — <https://quod.lib.umich.edu/i/icmc/bbp2372.1996.071/1>
   Foundational framework: PhISAM (modal synthesis for resonant percussion) + PhISEM (stochastic event modeling for granular sounds). Core reference for Strategies C + D.

3. **O'Brien, J.F., Cook, P.R., Essl, G. — "Synthesizing Sounds from Physically Based Motion" (SIGGRAPH 2001)**
   — <https://soundlab.cs.princeton.edu/publications/2001_siggraph_ssp.pdf>
   Physically-based sound synthesis from dynamic simulation of deformable bodies. FEM + modal synthesis for realistic impact sounds.

4. **van den Doel, K., Pai, D.K. — "Synthesis of Shape Dependent Sounds with Physical Modeling" (ICAD 1996)**
   — <https://www.icad.org/websiteV2.0/Conferences/ICAD96/proc96/dendoel.htm>
   First work incorporating impact location dependent sonic information in virtual environments. Modal synthesis with shape-dependent eigenfrequencies.

5. **Cook, P.R. — "Real Sound Synthesis for Interactive Applications" (Book, A.K. Peters 2002)**
   Comprehensive textbook on physically-informed sonic modeling, modal synthesis, granular synthesis for interactive applications.

## Tier 2 (footstep/contact synthesis & game audio)

6. **Turchet, L., Serafin, S., Dimitrov, S., Nordahl, R. — "Physically Based Sound Synthesis and Control of Footsteps Sounds" (DAFx 2010)**
   — <https://dafx10.iem.at/proceedings/papers/TurchetSerafinDimitrovNordahl_DAFx10_P50.pdf>
   Footstep synthesizer using physical models: modal synthesis for solid surfaces + PhiSM stochastic for aggregate (gravel, snow, sand) + friction + liquid models.

7. **Turchet, L. — "Footstep Sounds Synthesis: Design, Implementation, and Evaluation" (Applied Acoustics 2016)**
   — <https://www.sciencedirect.com/science/article/abs/pii/S0003682X15001747>
   Comprehensive footstep synthesis engine: solid/aggregate/liquid/hybrid materials, shoe types, anthropomorphic features.

8. **Nordahl, R., et al. — "Sound Synthesis and Evaluation of Interactive Footsteps for Virtual Reality Applications" (IEEE VR 2010)**
   — <https://www.lucaturchet.it/PUBLIC_DOWNLOADS/publications/journals/Sound_synthesis_and_evaluation_of_interactive_footsteps_and_environmental_sounds_rendering_for_virtual_reality_applications.pdf>
   VR footstep synthesis with modal models + physical models of contact mechanics (Hunt-Crossley force model).

9. **garjan — Environmental and nature sound synthesis for Rust** (2026)
   — <https://github.com/MacCracken/garjan>
   Production crate: modal synthesis (4-12 resonant modes per material), 10 materials for impacts, 8 terrains for footsteps, granular synthesis. Performance: Impact (Metal) = 1.4 ms for 1s audio (710× real-time). Direct production reference for performance targets.

10. **Procedural Footstep Synthesizer System — Unity Asset Store** (2025-2026)
    — <https://marketplace.unity.com/packages/tools/audio/procedural-footstep-synthesizer-system-371308>
    Commercial Unity asset: 4-layer DSP engine (transient + modal resonance + texture/drag + wetness). Production validation of physics-based footstep synthesis.

## Tier 3 (material properties & acoustic physics)

11. **IRCAM Modalys — Material Properties Tables**
    — <https://support.ircam.fr/docs/Modalys/3.8.0/Objects/ObjectProperties/object_properties_material.html>
    Density, Young's modulus, Poisson ratio for 50+ materials (metals, fibers, woods, plastics, liquids). Direct source for material acoustic parameter mapping.

12. **Barlow, C.Y. — "Wood for Sound" (American Journal of Botany 2006)**
    — <https://bsapubs.onlinelibrary.wiley.com/doi/10.3732/ajb.93.10.1439>
    Correlation of density, Young's modulus, loss coefficient for acoustical performance of materials.

13. **Brémaud, I. — "Acoustical Properties of Wood in String Instruments Soundboards" (HAL 2012)**
    — <https://hal.science/file/index/docid/808347/filename/Acoustical_properties_wood_Bremaud.pdf>
    Internal friction (tan δ), specific modulus (E'/ρ), quality factor for 441 wood species.

14. **Malcom3D/pbrAudioShaders — Physically Based Rendered Audio Shaders** (2025)
    — <https://github.com/Malcom3D/pbrAudioShaders>
    Hertzian contact theory + modal synthesis + material-aware (Young's modulus, density, damping). Multi-object collision support, impact/scraping/sliding/rotation sounds.

## Cross-references (closed experiments)

- `2026-06-21-audio-diffraction-hybrid` [mixed] — diffraction term for audio ray tracing; **orthogonal** (spatial propagation, not material interaction)
- `2026-06-21-audio-raytracing-voxel-sdf` [in-progress] — geometric audio path tracing; **orthogonal** (spatial propagation, not material interaction)
- `2026-06-22-procedural-engine-sound` [in-progress] — engine sound synthesis; **orthogonal** (vehicle-specific DSP, not voxel material interaction)
- `2026-06-21-ballistic-crack-thump` [closed] — supersonic projectile audio; **orthogonal**
- `2026-06-22-radio-communication-audio` [in-progress] — radio comms; **orthogonal**
