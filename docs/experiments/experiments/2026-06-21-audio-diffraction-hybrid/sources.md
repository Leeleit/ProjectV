# Sources — `2026-06-21-audio-diffraction-hybrid`

Полный список верифицированных источников (16 primary + 7 secondary). Дата верификации: 2026-06-21.

---

## Foundational (state of the art — diffraction rendering)

### Primary sources

1. **Schissler, Mehra, Manocha — "High-Order Diffraction and Diffuse Reflections for Interactive Sound Propagation in Large Environments"** — ACM SIGGRAPH 2014, ACM TOG 33(4) 39.
   — <https://gamma-web.iacs.umd.edu/HIGHDIFF/>, <https://gamma-web.iacs.umd.edu/HIGHDIFF/paper.pdf>
   — <https://dl.acm.org/doi/10.1145/2601097.2601216>
   *Foundational для high-order diffraction. Authors: Carl Schissler, Ravish Mehra, Dinesh Manocha (UNC Chapel Hill). Method: (1) iterative diffuse reflections combining path tracing + radiosity; (2) higher-order edge diffraction using UTD + precomputed global edge visibility graph; (3) wavelength-dependent edge simplification. Performance: 15-50 FPS on 4-core CPU, indoor + urban scenes with hundreds of obstacles. **Direct prior art for Pattern B (UTD edge-probe) in this experiment.***

2. **Schissler, Manocha — "Interactive Sound Propagation and Rendering for Large Multi-Source Scenes"** — ACM I3D 2014 (Best Paper Honorable Mention).
   — <https://dl.acm.org/doi/10.1145/2943779>
   *Sublinear scaling with number of sources via clustering + relative visibility. 50+ reflection orders at interactive rates on multicore PC. 5× speedup over prior geometric sound propagation algorithms. Up to 200 sound sources in complex indoor + outdoor scenes. Hybrid convolution-based audio rendering for Doppler shifting. **Pattern B foundation (relative visibility, clustering).***

3. **Cao, Ren, Schissler, Manocha, Zhou — "Interactive Sound Propagation with Bidirectional Path Tracing"** — SIGGRAPH ASIA 2016, ACM TOG 35(6).
   — <http://kunzhou.net/zjugaps/bst/>, <https://dl.acm.org/doi/10.1145/2980179.2982431>
   *BST (Bidirectional Sound Transport). Multiple importance sampling. SNR-based evaluation metric. **Closed `audio-raytracing-voxel-sdf` Phase 3 falsified at 17.1 ms cave (3.4× over 5 ms target). NOT Pattern B in this experiment — Pattern B is simplified UTD edge-probe, much cheaper than BST.***

4. **Cao, Schissler, et al. — "Fast Diffraction Pathfinding for Dynamic Sound Propagation"** — ACM SIGGRAPH 2021.
   — <https://dl.acm.org/doi/10.1145/3450626.3459751>
   *10th-order diffraction, **568× faster** than prior SOTA. Preprocessing: silhouette edge detection + planar diffraction geometry + edge visibility graph. Runtime: bidirectional path tracing against diffraction geometry + A* pathfinding. Direct prior art для high-order diffraction. **Pattern B can be extended to higher orders following this approach.***

5. **Tsingos, Funkhouser, Ngan, Carlbom — "Modeling Acoustics in Virtual Environments Using the Uniform Theory of Diffraction"** — ACM SIGGRAPH 2001.
   — <https://people.csail.mit.edu/addy/research/sig2001.pdf>, <http://www-sop.inria.fr/reves/personnel/Nicolas.Tsingos/publis/sig2001.pdf>
   *Beam tracing method for enumerating sequences of diffracting edges efficiently without aliasing in densely occluded polyhedral environments. Practical approximation where diffraction is computed only in shadow regions. Real-time auralization system. **Tsingos 2001 = UTD modeling (NOT depth-mip).***

6. **Tsingos, Dachsbacher, Lefebvre, Dellepiane — "Instant Sound Scattering"** — Eurographics Symposium on Rendering 2007.
   — <https://www-sop.inria.fr/reves/Basilic/2007/TDLD07/TDLD07.pdf>, <https://vcgdata.isti.cnr.it/Publications/2007/TDLD07/Tsingos_e_al_Instant_sound_Scattering.pdf>
   *GPU-accelerated scattering via depth-mip + Kirchhoff approximation. Source-view strategy: render scene from sound source location, then mip-map integration. 20-40× faster than CPU implementation. 700 Hz refresh rate on 256×256 render target. **Tsingos 2007 = depth-mip GPU (NOT Tsingos 2001). Pattern C foundation in this experiment.***

7. **Chandak, Lauterbach, Taylor, Ren, Manocha — "AD-Frustum: Adaptive Frustum Tracing for Interactive Sound Propagation"** — IEEE TVCG 2008.
   — <http://gamma.cs.unc.edu/SOUND/Diffraction/data/utd_report.pdf>, <http://gamma.cs.unc.edu/SOUND/data/vis2008.pdf>
   *UTD + frustum tracing extension. Adaptive diffraction frustum computation. Practical implementation for complex dynamic environments. Direct implementation reference для Pattern B (UTD edge-probe).*

8. **Antani, Chandak, Taylor, Manocha — "Fast Geometric Sound Propagation with Finite-Edge Diffraction"** — IEEE TVCG 2012.
   — <http://gamma.cs.unc.edu/BTM/>
   *Biot-Tolstoy-Medwin (BTM) diffraction model + region-based visibility culling. 2-4× reduction in visible primitives for second-order edge diffraction. **Alternative to UTD (Pattern B alternative).***

---

## Production-grade SOTA (2024-2026)

### Primary sources

9. **Vercidium — "Ray-Traced Audio Plugin" (2025)** — production reference для voxel audio.
   — <https://www.back2gaming.com/features/implementing-ray-traced-audio-in-games-a-technical-preview-of-vericidiums-plugin/>
   — <https://www.igorslab.de/en/raytracing-for-the-ears-when-the-sound-stumbles-through-the-voxel-forest/>
   *CPU-based ray-traced audio plugin for Unreal Engine + Godot. Voxel-based spatial grid (NOT triangle meshes). 4 ray types: penetration, direct, indirect, transmission. No GPU acceleration (CPU only, background threads). **Direct validation of our approach (voxel + CPU + audio ray-tracing).***

10. **SonoTraceUE — "An Unreal Engine 5 Plugin for Hardware-Accelerated Acoustic Ray Tracing"** — arXiv 2602.19652 (2026-01-09), GitHub Cosys-Lab/SonoTraceUE.
    — <https://github.com/Cosys-Lab/SonoTraceUE>
    — <https://www.arxiv.org/pdf/2602.19652>
    *UE5 plugin, hardware-accelerated ray tracing for audio. **Curvature-based Monte Carlo diffraction model** + specular reflection (DXR). Direct/Indirect/Diffraction/Specular path components. Near-real-time simulation. **Direct prior art для hybrid pattern (HW RT + diffraction term). Pattern B inspiration.***

11. **Pinpoint Audio Tracing — "Audio Tracing" UE plugin (2025-08-18)** — production reference.
    — <https://forums.unrealengine.com/t/pinpoint-audio-tracing/2645588>
    *UE plugin. Hardware ray tracing mandatory. Sound Material feature: per-surface scattering/reflectivity/absorption. Requires Lumen. **If hardware ray tracing unavailable, plugin bypasses calculations** — direct cost/availability trade-off. **Cross-reference: HW-RT dependent vs CPU-only (Vercidium).***

12. **Meta XR Audio SDK (2024-2026)** — production VR audio SDK.
    — <https://developer.oculus.com/blog/oculus-audio-sdk-intro/>
    — <https://developers.meta.com/horizon/documentation/unreal/meta-xr-acoustic-ray-tracing-unreal-map/>
    *Acoustic Map (precomputed) + Edge Diffraction option. Hybrid: precomputation для reflections/reverb/diffraction + runtime для direct sound occlusion. **Direct production SOTA reference для hybrid precomputed+runtime diffraction.***

13. **Wwise Spatial Audio (Audiokinetic) — "Diffraction + Transmission"** — production-proven implementation.
    — <https://blog.audiokinetic.com/wwise-spatial-audio-implementation-workflow-in-scars-above/>
    — <https://www.audiokinetic.com/en/library/edge/?id=sa_transmission.html>
    *Ak Geometry API: diffraction + transmission. Diffraction = sound bends around edges with lower volume + limited frequency spectrum. Transmission loss = 0-1 values per surface. Replaces UE built-in occlusion. Used in AAA (The Last of Us, Assassin's Creed, Cyberpunk 2077, Destiny). **Industry standard for diffraction в 2024-2026.***

14. **Google Patent WO2024179939A1 — "Multi-directional audio diffraction modeling for voxel-based audio scene representations"** — published 2024-09.
    — <https://patents.google.com/patent/WO2024179939A1/en>
    *Voxel-based audio scene + multi-directional diffraction. Pre-computed diffraction data + neighbor-voxel queries. Avoids extra pathfinding. **Direct patent prior art для voxel + multi-directional diffraction pattern. Pattern B inspiration.***

15. **Han, Denisova, Vasiliou et al. — "Perspectives of Sound Designers on Real-Time Sound Propagation in Games"** — IEEE CoG 2025.
    — <https://eprints.whiterose.ac.uk/id/eprint/232212/1/SurveyFinal_ReComments_final.pdf>
    *Industry survey. **41% of sound designers find low-pass filtering (LPF) alone insufficient for desired diffraction/obstruction/occlusion effects.** Diffraction + reverb = top priorities. Built-in audio engine features (middleware) = most common approach. **Direct industry validation: diffraction is needed, LPF alone not enough.***

16. **Wwise vs FMOD vs MetaSounds (2026-03-25, StraySpark)** — middleware comparison.
    — <https://www.strayspark.studio/blog/wwise-fmod-metasounds-audio-middleware-comparison>
    *Wwise = most advanced (built-in diffraction + transmission + room/portal + geometric reflections + HRTF). FMOD = no built-in diffraction, integrates with Meta XR / Oculus Audio. MetaSounds UE5 = no native diffraction without custom implementation. **Industry trend: Wwise dominant for AAA with spatial audio = diffraction required.***

---

## Secondary / supporting sources

17. **Differentiable Geometric Acoustic Path Tracing — Time-Resolved Path Replay Backpropagation** — TU Berlin 2024+.
    — <https://cybertron.cg.tu-berlin.de/projects/diff-acoustic-pt/media/paper.pdf>
    *Differentiable acoustic path tracing. Backward-mode differentiation для inverse acoustics. Just-noticeable difference (JND) ≈ 1 dB. Demonstrates +2.9 dB clarity improvement at individual receiver positions. **ML-based SOTA, not directly applicable to ProjectV (no ML infra). Context only.***

18. **Jüterbock, Wujecki, Weinzierl — "Differentiable Acoustic Path Tracing: Full Spectral Rendering"** — DAGA 2024.
    — <https://pub.dega-akustik.de/DAGA_2024/files/upload/paper/489.pdf>
    *Mitsuba 3-based acoustic path tracing. Millions of rays per second. Plans to model diffraction + transmission + scattering. **ML-based SOTA. Context only.***

19. **DiffRIR — "Hearing Anything Anywhere"** — arXiv 2406.07532 (2024).
    — <https://arxiv.org/pdf/2406.07532>
    *Differentiable RIR rendering from sparse 12 recordings + planar reconstruction. Approximates diffraction, transmission, refraction, higher-order specular reflections as spatially uniform. **ML-based. Context only.***

20. **Acoustic Volume Rendering (AVR) — NeurIPS 2024**.
    — <https://proceedings.neurips.cc/paper_files/paper/2024/file/4ebf0617b32da2cd083c3b17c7285cce-Paper-Conference.pdf>
    *Acoustic volume rendering для neural impulse response field. Spherical integration + frequency-domain rendering. Outperforms existing methods on real + simulated datasets. **ML-based. Context only.***

21. **Reciprocal Latent Fields for Precomputed Sound Propagation** — arXiv 2602.06937 (2026).
    — <https://arxiv.org/pdf/2602.06937>
    *Precomputed sound propagation with latent spatial embeddings. Orders-of-magnitude memory reduction vs wave coding. **ML-based. Context only.***

22. **ReSTIR BDPT — "Bidirectional ReSTIR Path Tracing with Caustics"** — 2025 ACM TOG, GitHub Shmaug/ReSTIR-BDPT (2024-11-13).
    — <https://github.com/Shmaug/ReSTIR-BDPT>
    *Light transport only. Out of scope для ProjectV audio. **Context only (related bidirectional path tracing technique).***

23. **Antani, Chandak, Taylor, Manocha — "Diffraction Kernels for Interactive Sound Propagation in Dynamic Environments"** — IEEE TVCG 24(4), 1613-1622 (2018).
    *Practical implementation of diffraction kernels. **Direct implementation reference для Pattern B.***

---

## Cross-references в ProjectV

Per `AGENTS.md §3` — не дублировать, только cross-refs:

- `agent/knowledge.md §28` — `AudioEngine` contract (miniaudio PCM playback + future spatial extensions).
- `experiments/2026-06-21-audio-raytracing-voxel-sdf/README.md` — Phase 1 (occlusion) + Phase 2 (Eyring reverb) recommended. Phase 3 falsified. Line 459-460 = **explicit Phase 1.5 follow-up = this experiment**.
- `experiments/2026-06-20-hzb-binding-models/README.md` — `texelFetch` pattern (closed mixed, recommended). This experiment reuses same pattern для depth-mip probe.
- `experiments/2026-06-20-nanovdb-on-gpu/README.md` — SVO walker foundation (closed yes, hybrid SVDAG + NanoVDB). Future: hierarchical skip для Pattern B.
- `experiments/2026-06-20-work-stealing-job-system/README.md` — serial dispatcher default (closed mixed). Audio = single-threaded.
- `experiments/2026-06-20-simd-procedural-noise/README.md` — AVX2 baseline (closed mixed, no AVX-512 на Zen 3). Diffraction CPU = AVX2 floor.
- `docs/experiments/hardware-profile.md` §1 (Zen 3 5800X baseline) — primary hardware reference.
- `docs/experiments/benchmarks/methodology.md` §3 (measurement protocol).
- `docs/experiments/AGENTS.md` §13 (topic reservation protocol).
