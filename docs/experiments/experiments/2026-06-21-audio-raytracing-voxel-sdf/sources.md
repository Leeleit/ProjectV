# Sources — audio-raytracing-voxel-sdf

Web-research conducted `2026-06-21`, 3 batch queries (per `docs/experiments/AGENTS.md §4`). 12 key sources верифицированы.

## Primary (production-validated voxel-grid pattern)

1. **Vercidium Audio** (2025, production) — Real-time raytraced audio для games. Voxel-based spatial grid для CPU ray
   tracing, 32 rays/frame update rate на background thread, occlusion + permeation + EAX reverb. C SDK + Unreal Engine
   plugin planned late 2026. WebAssembly sandbox available.
   - <https://vercidium.com/>
2. **igorslab.de review** — "Ray tracing for the ears: When sound stumbles through the voxel forest" by Samir Bashir,
   2025-04-17. Reviews Vercidium system, validates voxel-based approach как production-feasible на CPU без RTX constraints.
   - <https://www.igorslab.de/en/raytracing-for-the-ears-when-the-sound-stumbles-through-the-voxel-forest/>

## Academic SOTA (2024-2025)

3. **Differentiable Geometric Acoustic Path Tracing** (Finnendahl, Worchel, Jüterbock, Wujecki, Brinkmann, Weinzierl,
   Alexa) — **SIGGRAPH 2025**, ACM TOG 44:4 (August 2025), doi:10.1145/3730900. Time-Resolved Path Replay Backpropagation
   для audio, constant memory + linear time. Validates acoustic rendering equation = light transport equation duality.
   - <https://cybertron.cg.tu-berlin.de/projects/diff-acoustic-pt/>
   - <https://cybertron.cg.tu-berlin.de/projects/diff-acoustic-pt/media/paper.pdf>
4. **NeRAF: 3D Scene Infused Neural Radiance and Acoustic Fields** (Briot et al.) — **ICLR 2025**. Jointly learns acoustic
   + radiance fields, validates audio-visual alignment = active research area. (Not directly applicable к real-time CPU
   path, but confirms research relevance.)
   - <https://proceedings.iclr.cc/paper_files/paper/2025/hash/e84aaafaf35a7e2b4389dfa22b0889c4-Abstract-Conference.html>

## Open-source reference implementations

5. **GSound-SIR** (Mar 2025, arXiv 2503.17866) — Python toolkit поверх GSound C++ ray tracer. NVIDIA OptiX support
   added Dec 2025. Energy-based filtering, Parquet storage, up to 9th-order Ambisonics.
   - <https://arxiv.org/pdf/2503.17866>
   - <https://github.com/yongyizang/GSound-SIR>

## Foundational geometric acoustic algorithms (validated)

6. **Interactive Sound Propagation and Rendering for Large Multi-Source Scenes** (Schissler, Mehra, Manocha) —
   2014, ACM TOG (doi:10.1145/2943779). High-order diffraction + diffuse reflections. **50 reflection orders at
   interactive rates**, 5× speedup over prior algorithms, 200 sound sources validated.
   - <https://dl.acm.org/doi/10.1145/2943779>
7. **Interactive Sound Propagation with Bidirectional Path Tracing** (Schissler, Manocha et al.) — 2014,
   ACM SIGGRAPH / TOG, doi:10.1145/2980179.2982431. BST algorithm с multiple importance sampling, sublinear source
   scaling via clustering.
   - <https://dl.acm.org/doi/10.1145/2980179.2982431>
8. **RESound** (Lentz, Schröder, Vorländer, Assenmacher) — 2007. Hybrid ray-frustum + stochastic ray tracing +
   statistical late reverb. Handles tens of thousands of scene primitives.
   - <http://gamma-web.iacs.umd.edu/Sound/RESound/RESound.pdf>
9. **iSound: Interactive GPU-based Sound Auralization in Dynamic Scenes** (Raghuvanshi, Lin) — GPU compute path,
   validates что CPU path sufficient для moderate scene complexity (referenced для cross-validation cost).
   - <http://gamma-web.iacs.umd.edu/Sound/iSound/isound-tech_report.pdf>
10. **High-Order Diffraction and Diffuse Reflections for Interactive Sound Propagation in Large Environments**
    (Schissler, Mehra, Manocha) — 2014, ACM TOG 33:4. Order of magnitude improvement vs prior diffuse reflection
    algorithms.
    - <http://gamma-web.iacs.umd.edu/HIGHDIFF/paper.pdf>

## Graphics-hardware-accelerated acoustic occlusion/diffraction

11. **Fast rendering of sound occlusion and diffraction effects for virtual acoustic environments** (Tsingos) — 2001,
    AES 104, INRIA. Validates hardware-accelerated traversal via graphics pipelines (depth-map approach), applicable
    к ProjectV HZB-style traversal.
    - <http://www-sop.inria.fr/reves/personnel/Nicolas.Tsingos/publis/aes104.pdf>

## Architectural environments + beam tracing

12. **Modeling Sound Reflection and Diffraction in Architectural Environments with Beam Tracing** (Funkhouser,
    Carlbom, Elko, Pingali, Sondhi, West) — 2002, Princeton. Beam tracing для architectural environments, validates
    polyhedral beam propagation для early reflections в densely-occluded scenes (relevant для cave scenes).
    - <https://www.cs.princeton.edu/~funk/sevilla02.pdf>

## Production VR audio ray tracing

13. **Meta Acoustic Ray Tracing (Audio SDK)** — Meta Horizon OS, 2024+. Acoustic Ray Tracing features для Unity/Unreal
    VR. Simulates reflections, diffraction, occlusion, obstruction в real-time на mobile VR hardware. Validates что
    production VR systems ship geometric audio.
    - <https://developers.meta.com/horizon/blog/acoustic-ray-tracing-audio-sdk-meta-quest-developer-social-presence/>

## Cross-validation per ProjectV context

- **Vercidium 2025** → directly validates voxel-grid CPU approach (matches our prototype).
- **Schissler 2014** → validates high reflection orders feasible (50 orders measured), but на smaller scale scenes.
- **Meta Audio SDK** → validates production shipping (VR ready, mobile ready).
- **Finnendahl SIGGRAPH 2025** → validates acoustic rendering equation duality with light transport, suggests future
  optimization via Path Replay Backpropagation (differentiable audio rendering for inverse problems).
- **GSound-SIR** → open-source reference для future GPU compute path, если CPU не хватает (>1000 sources).

## Coverage map (per SOTA 2024-2026)

| Period | Algorithm | Author/Project | Status | Relevance to ProjectV |
|:-------|:----------|:---------------|:-------|:----------------------|
| 2025 (prod) | Voxel-grid CPU ray tracing | Vercidium Audio | Production | **Direct match** — primary inspiration |
| 2025 (academic) | Differentiable geometric PT | Finnendahl et al. SIGGRAPH | Research | Future: optimization via PRB |
| 2025 (OSS) | GSound + OptiX | GSound-SIR | Open-source | Reference impl, GPU compute future |
| 2024 (prod) | VR acoustic ray tracing | Meta Audio SDK | Production | Validation: shipping product |
| 2014 | High-order diffraction | Schissler & Manocha | Academic | Validation: 50 orders at interactive rates |
| 2007 | Hybrid ray-frustum | RESound | Academic | Validation: hybrid geometric + statistical pattern |
| 2002 | Beam tracing | Funkhouser et al. | Academic | Validation: architectural scene ray tracing |
| 2001 | HW-accelerated occlusion | Tsingos | Academic | Validation: GPU pipeline traversal applicable |

## Cited by ProjectV docs (cross-refs)

- `agent/knowledge.md §28` — AudioEngine contract (miniaudio, 16/44100, no geometric processing).
- `2026-06-20-nanovdb-on-gpu` — SVO walker foundation.
- `2026-06-20-flecs-soa-vs-aos-bench` — SoA storage for AudioSource / AudioListener components.
- `2026-06-20-work-stealing-job-system` — serial dispatcher baseline.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold.
- `hardware-profile.md §1` — Zen 3 5800X baseline.
- `TODO.md` — current stages (no audio stage; this experiment opens Stage 7.x).
