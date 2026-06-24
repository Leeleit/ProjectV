# Sources — 2026-06-21-taa-motion-vectors

Web-research per `docs/experiments/AGENTS.md §4` (Exa + verification). Captured `2026-06-21`.

---

## Primary references

### [Karis 2014 "High Quality Temporal Supersampling"](https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf)

Brian Karis, Epic Games. SIGGRAPH Advances in Real-Time Rendering for Games course, 2014-08-11.
**Foundational Unreal Engine 4 TAA paper.** Most-cited TAA reference in industry.

Key technical claims (verified by reading paper):

- **«16:16 RG velocity buffer»** = R16G16_SFLOAT = exactly the format `TODO.md §5.3` line 425 prescribes for motion
  vector MRT.
- **«Velocity accuracy is super important — Minor imprecision will streak a static image»** — drives
  recommendation for vertex-out (Pipeline A) over depth-reproject (Pipeline B) because depth-reproject has
  fundamental precision loss near edges of dynamic objects.
- **«Use same velocity buffer calculation as motion blur»** — establishes the connection between TAA motion vectors
  and motion blur (TODO §5.3 line 425 also mentions both as related).
- **YCoCg color space** for color clamping (better for HDR content than RGB clamping).
- **Neighborhood clamping (3x3 AABB)** as primary ghosting mitigation.
- **«Responsive AA» material flag** with stencil-based feedback reduction for translucency (advanced, out of scope
  for this experiment).
- **«First anti-flickering attempt» via historical variance in alpha channel** — abandoned, replaced with blend
  factor reduction.

**Takeaway:** Karis 2014 = the standard reference for TAA implementation. Pipeline A (vertex-out) + Karis
neighborhood clamping = exactly the approach Karis recommends. Pipeline B (depth-reproject) = "Brute Force"
alternative Karis mentions for compatibility with non-motion-vector renderers (older engines, no MRT).

---

### [Yang/Liu/Salvi 2024 "A Survey of Temporal Antialiasing Techniques"](https://www.leiy.cc/publications/TAA/TemporalAA.pdf)

L. Yang, S. Liu, M. Salvi. Stanford / Activision Research, 2024.
**Comprehensive survey of TAA techniques 2011-2024.** Most up-to-date reference.

Key technical claims (verified by reading survey):

- Confirms **neighborhood clamping** (Karis 2014, Salvi 2016) as standard ghosting mitigation. This experiment
  uses 3x3 AABB clamping per this baseline.
- **«Since motion vectors cannot be antialiased, reprojection using motion vectors may reintroduce aliasing
  artifacts to smooth, antialiased edges along object boundaries of moving objects. A simple approach to avoid
  such artifacts is to dilate the foreground objects when sampling motion vectors, so that all boundary pixels
  touched by the edge are reprojected along with these objects [Kar14]. Typically, a small 4-tap dilation window
  is used.»** — this experiment's Pipeline A implements the 4-tap dilation per Karis 2014.
- **«An adaptive scheme has also been proposed by Wihlidal [Wih17] to only fetch and compare depth value when
  motion vectors diverge»** — adaptive scheme (Wihlidal 2017 Frostbite) = future improvement, out of scope.
- Survey covers all major TAA variants: Lottes 2011, Karis 2014, Salvi 2016, Jimenez 2016, Epic 2015, Wihlidal 2017,
  Pedersen 2017, Yang 2020, Riley/Arcila 2022, Vaidyanathan 2023.

**Takeaway:** survey confirms Pipeline A + 3x3 AABB clamping + YCoCg = current SOTA baseline (still optimal in
2024). k-DOP / adaptive ray tracing / DLSS-style are advanced variants worth separate investigation.

---

### [Marrs/Spjut/Gruen/Sathe/McGuire 2018 "Adaptive Temporal Antialiasing"](https://research.nvidia.com/sites/default/files/pubs/2018-08_Adaptive-Temporal-Antialiasing/adaptive-temporal-antialiasing-preprint.pdf)

Adam Marrs, Josef Spjut, Holger Gruen, Rahul Sathe, Morgan McGuire. NVIDIA. HPG 2018.

Key technical claims (verified by reading paper):

- **Failure segmentation mask** identifies where TAA will fail; ray-traced alternatives used at failure pixels.
- Tries to **eliminate heuristic tuning** by replacing clamping with adaptive ray tracing.
- **«0.5 ms» measured cost** on DXR-capable hardware at the time. Includes actual ray tracing for disocclusion
  handling.
- Designed for **DXR / ray-tracing capable hardware** (NVIDIA RTX 20+ at the time of writing, 2018).
- **Not suitable for ProjectV baseline TAA** — requires ray tracing infrastructure (Stage 5.2+ RT, future per
  `rt-shadows-vs-csm` experiment).

**Takeaway:** represents SOTA with ray tracing available, but **out of scope** for Stage 5.3 TAA baseline. Could
be follow-up experiment when Stage 5.2 RTX shadows land. Mention for completeness.

---

### [Marrs et al. 2019 "Improving Temporal Antialiasing with Adaptive Ray Tracing"](https://research.nvidia.com/sites/default/files/pubs/2019-03_Improving-Temporal-Antialiasing/Marrs2019_Chapter_TemporalAntialiasingWAdaptiveRays.pdf)

Adam Marrs et al. NVIDIA. Book chapter 2019.

Key technical claims:

- **«achieves quality approaching 16× supersampling of geometry, shading, and materials within the 16 ms frame
  budget required of most games»** — RTX substantially improves TAA quality.
- Direct application to UE4 + DXR.

**Takeaway:** confirms Marrs 2018 follow-up. RTX + TAA = 16× supersampling quality at 16 ms budget. Same out-of-scope
constraint.

---

### [k-DOP Clipping SIGGRAPH 2024](https://dl.acm.org/doi/10.1145/3681758.3697996)

**k-Discrete Oriented Polytopes for Robust Ghosting Mitigation in Temporal Antialiasing.** SIGGRAPH 2024 paper.

Key technical claims (verified via abstract + highlights):

- **«For a 0.2 ms performance overhead, our method more reliably mitigates ghosting across scenes where previous
  methods have inconsistent results»** — 32-DOPs provides best anti-ghosting vs shimmer tradeoff.
- Replaces 3x3 AABB clamping with k-DOPs (Discrete Oriented Polytopes with k directions).
- **«Temporal stability and minimal ghosting are often contradictory goals»** — important caveat for tuning.
- **«Comparing to (non-temporally) supersampled reference images would introduce a constant error by also
  comparing general TAA quality to supersampling. We compare 100 frames of animation to the aliased ground truth
  by using the average value of the mean FLIP over all frames»** — uses FLIP metric (image difference perceptual
  metric) for measurement.

**Takeaway:** SOTA ghosting mitigation 2024. **Could be follow-up experiment** to replace 3x3 AABB clamping with
32-DOPs in this experiment's TAA resolve. 0.2 ms cost = acceptable.

---

### [Karolewics Lumberyard "Anti-Ghosting TAA"](https://stevekarolewics.com/articles/anti-ghosting-taa.html)

Steve Karolewics, The Grand Tour Game (Amazon Game Studios). 2018.

Key technical claims (verified by reading article):

- **«On Xbox One, this added about 0.1ms to our TAA shader for a total runtime cost of about 1.6ms on the GPU»**
  — production anti-ghosting TAA cost.
- **«Developed a new anti-ghosting TAA technique that uses a blend of pixel depth and pixel motion to determine
  how to blend the pixel history»** — depth+motion-blending = Karis 2014 approach.
- **«Modified the camera's projection matrix with a sub-pixel jitter each frame»** — standard TAA jitter pattern.
- Reference to Naughty Dog's prior improvements (depth-aware blend) + Karis 2014 + Salvi 2016.

**Takeaway:** **production reference** for anti-ghosting TAA at console GPU budget (1.6 ms total on Xbox One).
Confirms Karis 2014 + depth-aware blending is the standard production approach. This experiment's Karis 2014
implementation + Pipeline A motion vector MRT is on the same architectural path.

---

### [VK_KHR_dynamic_rendering Vulkan Spec](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_dynamic_rendering.html)

Khronos, ratified 2021-10-06 by Tobias Hector (AMD) + Arseny Kapoulkine (Roblox) + François Duranleau (Gameloft) +
Stuart Smith (AMD) + 9 others. **Promoted to core in Vulkan 1.3.**

Key technical claims (verified by reading spec):

- **Replaces `VkRenderPass` + `VkFramebuffer` with `VkRenderingInfo` + `VkRenderingAttachmentInfo`.**
- Pipeline creation uses **`VkPipelineRenderingCreateInfoKHR`** pNext with `colorAttachmentCount` +
  `pColorAttachmentFormats` + `depthAttachmentFormat`.
- **Multiple color attachments supported** for MRT (e.g., color + motion vector).
- Already ProjectV mainline per `agent/knowledge.md` + `hardware-profile.md §4`.

**Takeaway:** enables Pipeline A's MRT pattern. Vulkan 1.3 core (no extension needed on dev host).

---

### [Khronos Vulkan-Samples dynamic_rendering](https://github.khronos.org/Vulkan-Site/tutorial/latest/courses/18_Ray_tracing/01_Dynamic_rendering.html)

Reference implementation of dynamic rendering. Confirms:

- `vk::RenderingAttachmentInfo` with `.imageView`, `.imageLayout`, `.loadOp`, `.storeOp`, `.clearValue`.
- `vk::RenderingInfo` aggregates color + depth + stencil attachments.
- Pipeline creation pNext chain: `PipelineRenderingCreateInfo` → `GraphicsPipelineCreateInfo`.

**Takeaway:** canonical reference for dynamic rendering pattern. This experiment's prototype will follow this
pattern (modern C++ wrapper or raw Vulkan 1.4 API).

---

## Secondary references

- **[Wikipedia TAA article](https://en.wikipedia.org/wiki/Temporal_anti-aliasing)** — general overview, cross-references
  to Karis 2014, Salvi 2016, Halo: Reach, Crysis 2 history.
- **[Digital Foundry "TAA: a blessing or a curse?" 2024-02-11](https://www.digitalfoundry.net/articles/digitalfoundry-2024-temporal-anti-aliasing-a-blessing-or-a-curse)**
  — industry retrospective on TAA trade-offs (ghosting, blur, motion dependence). Useful for context on production
  tuning.
- **[Khronos Vulkan-Samples dynamic_rendering sample](https://github.com/KhronosGroup/Vulkan-Samples/blob/main/samples/extensions/dynamic_rendering/README.adoc)**
  — full code reference for dynamic rendering pattern.
- **`agent/knowledge.md`** — ProjectV mainline = Vulkan 1.3/1.4 with VK_KHR_dynamic_rendering (per existing
  knowledge).
- **`hardware-profile.md §4`** — dev host `obvium` supports all required extensions (VK_KHR_dynamic_rendering core 1.3,
  R16G16_SFLOAT format standard, VK_KHR_dynamic_rendering_local_read Vulkan 1.4 feature).

---

## Cross-references to ProjectV docs

- `TODO.md §5.3` TAA Motion Vectors (explicit format prescription: `VK_FORMAT_R16G16_SFLOAT`).
- `2026-06-20-dec-pipelines-async-compute` (closed verdict=yes) — async foundation for cross-frame pipelining.
- `2026-06-20-async-compute-overhead-numbers` (closed verdict=yes) — async overhead measurement (+9.85-11.34% measured
  on RTX 3060 Ti).
- `2026-06-20-clustered-forward-mass-lights` (closed verdict=yes) — Stage 5 lighting axis (SSBO light list source
  for TAA-aware per-fragment ray budget).
- `2026-06-20-vct-vs-rt-cutoff` (closed verdict=mixed) — Stage 5 GI strategy.
- `2026-06-20-rt-shadows-vs-csm` (closed verdict=mixed) — Stage 5.2 RTX shadows (foundation for Marrs 2018 adaptive
  TAA future work).
- `2026-06-20-restir-gi-feasibility` (closed verdict=mixed) — SOTA GI deferred to Stage 6+ post-MVP.
- `agent/knowledge.md` — 3-step migration precedent.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold.
- `docs/experiments/hardware-profile.md §3` (RTX 3060 Ti, 8 GiB VRAM) + §4 (dynamic rendering core 1.3).
- `docs/experiments/benchmarks/methodology.md` — measurement protocol.

---

## Verification notes

- All URLs verified accessible `2026-06-21`.
- Karis 2014 paper: `de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf` — fetched + read,
  16:16 RG velocity buffer claim confirmed.
- k-DOP Clipping SIGGRAPH 2024: `dl.acm.org/doi/10.1145/3681758.3697996` — abstract + highlights verified, 0.2 ms
  overhead + 32-DOPs best tradeoff confirmed.
- VK_KHR_dynamic_rendering: `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_dynamic_rendering.html` —
  ratified 2021-10-06, promoted to Vulkan 1.3 core, confirmed.
- Marrs 2018 HPG: `research.nvidia.com` — paper text + figures verified.
- Karolewics article: `stevekarolewics.com/articles/anti-ghosting-taa.html` — full text read, 0.1 ms cost +
  Naughty Dog prior improvements confirmed.
- `VK_FORMAT_R16G16_SFLOAT` as standard motion vector format — cross-validated by:
  (a) Karis 2014 "16:16 RG velocity buffer"
  (b) `TODO.md §5.3` line 425 explicit
  (c) UE 5 + Godot 4.x + Unity HDRP de facto standard (Karis 2014 trained at Epic, format adopted industry-wide)
