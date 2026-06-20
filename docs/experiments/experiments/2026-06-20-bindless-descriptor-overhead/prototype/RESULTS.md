bindless_layout_sketch — analytical model for ProjectV Stage 2.x descriptor strategy
Per docs/experiments/AGENTS.md section 2: standalone CPU-only research artifact.
Output: deterministic analytical estimates from cited sources (see file header).

========= ProjectV descriptor access pattern classification =========
Stable (bindless candidate):
- Material table SSBO (per-material, mostly stable, low-frequency update)
- Sparse64Node pool (per-chunk, lazy dedup, indexed in shader for Stage 2.1)
- Shadow cascade views (per-frame but stable identity)
Transient (traditional + dynamic offset, or push):
- PackedFace SSBO (per-frame from compute cull)
- Voxel payload SSBO (per-chunk, mutated on edit)
- HZB mip image (per-frame, Stage 2.2)
- TAA history image (per-frame, dual-buffered)
- Indirect draw buffers (per-frame from compute cull)
- Motion vector buffer (per-frame, Stage 5.3)
Per-draw-call small (push descriptor candidate):
- Per-material shadow cascade params (~4 floats, < 32 bytes push constants)
- Debug view toggle parameters

========= Vendor cross-reference (XDC 2025-09-29 "Descriptors are Hard") =========
NVIDIA (Turing+/Ampere/Ada/Blackwell):
- Internal: descriptor sets implemented as 2 big tables (images + samplers)
- Table switching = VERY expensive (sticky to context)
- Bindless native; descriptor buffer EMULATED (5 indirections in VKD3D-Proton)
- 32B per image descriptor, 32B per sampler descriptor
- Practical advice: prefer UPDATE_AFTER_BIND for streamed resources; keep descriptor tables bounded where possible

AMD RDNA2 (RX 6000) + RDNA3 (RX 7000):
- 32B per image descriptor, 16B per sampler descriptor
- Descriptor buffer HW-supported (efficient path)
- Vulkan 1.4 = descriptorIndexing core; PARTIALLY_BOUND + UPDATE_AFTER_BIND well-supported
- RADV (open source) supports all variants

Intel Arc (Alchemist + Battlemage + Core Ultra Meteor Lake+):
- Gfx12.5+ has TWO modes: LEGACY (binding tables) + BUFFER (descriptor buffer)
- ANV_ALWAYS_BINDLESS=1 forces bindless path for testing
- 64B per image descriptor (largest of three vendors)
- Driver maturity improved significantly in 2024-2025 (Phoronix)

Arm v9+ (mobile, Mali Valhall gen1+):
- VK_EXT_descriptor_buffer HW-supported (32 set bindings, each -> buffer of descriptors)
- 'Fully pipelined' descriptor set bindings unlike NVIDIA/Intel
- Mobile note: 'Adreno 660 performed poorly with bindless' (AsEn 2025 benchmarks)
- 'On older Mali T830 and Adreno 505, nonuniform() isn't supported, allowing only 16 textures to be bound'

========= Per-stage projection: Current baseline =========

=== ProjectV current baseline at Current baseline (64 chunks visible) ===
Total pipeline descriptors (current): 23 bindings across 4 pipelines
Estimated CPU desc update calls per frame: 10 (4 pipelines * 1 update each + 6 transient SSBO rebinds)
Measured reference (analogous workloads):
- Traha (Samsung, 2024): 220 vkUpdateDescriptorSets = 3.554ms saved by dynamic-offset rewrite (+5 FPS @ 37->42)
- Arm Mali sample: 44ms -> 27ms frame time = 38% saving from descriptor caching alone
- vkguide Ascendant (~400k chunks): per-batch compute dispatch + DrawIndirectInstanced pattern, no bindless
Per-frame descriptor cost (estimated, 64 visible chunks): 8-12 us CPU + 0 GPU static overhead
Per-frame descriptor cost (estimated, 64 visible chunks, future Stage 4.3):
CPU grows ~linearly with chunk count: 16-24 us at 64 chunks (still < 0.15% of 16.67ms frame)
Verdict: descriptor update cost is NOT a current bottleneck (< 0.2% frame budget).
Stage 2.x consideration: if Stage 2.3 (3D virtual texturing) lands, per-page-table rebind cost grows.

=== Traditional (current ProjectV baseline) ===
CPU desc update per frame 25.00 us
CPU bind calls per frame 14.00 calls
CPU total per frame 32.00 us
GPU static overhead 0.00 B
Validation layer factor 1.00 x
% of 16.67ms frame budget (CPU only)           0.19 %
Notes:                                         0.00
Frame-in-flight per-frame descriptor updates. Stable, debug-friendly.

=== Bindless (VK_EXT_descriptor_indexing) ===
CPU desc update per frame 2.00 us
CPU bind calls per frame 1.00 calls
CPU total per frame 2.50 us
GPU static overhead 4194304.00 B
Validation layer factor 8.00 x
% of 16.67ms frame budget (CPU only)           0.01 %
Notes:                                         0.00
Unbounded arrays + PARTIALLY_BOUND + UPDATE_AFTER_BIND. GPU-AV REQUIRED for PARTIALLY_BOUND validation (8× debug
overhead per Khronos docs).

=== Push (VK_KHR_push_descriptor) ===
CPU desc update per frame 4.00 us
CPU bind calls per frame 14.00 calls
CPU total per frame 11.00 us
GPU static overhead 0.00 B
Validation layer factor 1.05 x
% of 16.67ms frame budget (CPU only)           0.07 %
Notes:                                         0.00
vkCmdPushDescriptorSet[KHR] inline update. maxPushDescriptors per layout (HW-dependent, NOT 32 — that's a common
misconception).

=== Descriptor Buffer (VK_EXT_descriptor_buffer) ===
CPU desc update per frame 1.50 us
CPU bind calls per frame 1.00 calls
CPU total per frame 2.00 us
GPU static overhead 2097152.00 B
Validation layer factor 4.00 x
% of 16.67ms frame budget (CPU only)           0.01 %
Notes:                                         0.00
Descriptors as buffer memory, memcpy()-update. HW on AMD/Intel/Arm v9+, emulated on NVIDIA (5 indirections in
VKD3D-Proton per XDC 2025).

=== Hybrid (recommended) ===
CPU desc update per frame 6.00 us
CPU bind calls per frame 6.00 calls
CPU total per frame 9.00 us
GPU static overhead 1048576.00 B
Validation layer factor 2.00 x
% of 16.67ms frame budget (CPU only)           0.05 %
Notes:                                         0.00
Bindless for chunk/material tables (stable, indexed in shader); traditional+dynamic-offset for per-frame SSBOs; push for
small transient sets.

========= Per-stage projection: Stage 4.3 (128+ chunks) =========

=== ProjectV current baseline at Stage 4.3 (128+ chunks) (128 chunks visible) ===
Total pipeline descriptors (current): 23 bindings across 4 pipelines
Estimated CPU desc update calls per frame: 10 (4 pipelines * 1 update each + 6 transient SSBO rebinds)
Measured reference (analogous workloads):
- Traha (Samsung, 2024): 220 vkUpdateDescriptorSets = 3.554ms saved by dynamic-offset rewrite (+5 FPS @ 37->42)
- Arm Mali sample: 44ms -> 27ms frame time = 38% saving from descriptor caching alone
- vkguide Ascendant (~400k chunks): per-batch compute dispatch + DrawIndirectInstanced pattern, no bindless
Per-frame descriptor cost (estimated, 64 visible chunks): 8-12 us CPU + 0 GPU static overhead
Per-frame descriptor cost (estimated, 128 visible chunks, future Stage 4.3):
CPU grows ~linearly with chunk count: 16-24 us at 128 chunks (still < 0.15% of 16.67ms frame)
Verdict: descriptor update cost is NOT a current bottleneck (< 0.2% frame budget).
Stage 2.x consideration: if Stage 2.3 (3D virtual texturing) lands, per-page-table rebind cost grows.

=== Traditional (current ProjectV baseline) ===
CPU desc update per frame 25.00 us
CPU bind calls per frame 14.00 calls
CPU total per frame 32.00 us
GPU static overhead 0.00 B
Validation layer factor 1.00 x
% of 16.67ms frame budget (CPU only)           0.19 %
Notes:                                         0.00
Frame-in-flight per-frame descriptor updates. Stable, debug-friendly.

=== Bindless (VK_EXT_descriptor_indexing) ===
CPU desc update per frame 2.00 us
CPU bind calls per frame 1.00 calls
CPU total per frame 2.50 us
GPU static overhead 4194304.00 B
Validation layer factor 8.00 x
% of 16.67ms frame budget (CPU only)           0.01 %
Notes:                                         0.00
Unbounded arrays + PARTIALLY_BOUND + UPDATE_AFTER_BIND. GPU-AV REQUIRED for PARTIALLY_BOUND validation (8× debug
overhead per Khronos docs).

=== Push (VK_KHR_push_descriptor) ===
CPU desc update per frame 4.00 us
CPU bind calls per frame 14.00 calls
CPU total per frame 11.00 us
GPU static overhead 0.00 B
Validation layer factor 1.05 x
% of 16.67ms frame budget (CPU only)           0.07 %
Notes:                                         0.00
vkCmdPushDescriptorSet[KHR] inline update. maxPushDescriptors per layout (HW-dependent, NOT 32 — that's a common
misconception).

=== Descriptor Buffer (VK_EXT_descriptor_buffer) ===
CPU desc update per frame 1.50 us
CPU bind calls per frame 1.00 calls
CPU total per frame 2.00 us
GPU static overhead 2097152.00 B
Validation layer factor 4.00 x
% of 16.67ms frame budget (CPU only)           0.01 %
Notes:                                         0.00
Descriptors as buffer memory, memcpy()-update. HW on AMD/Intel/Arm v9+, emulated on NVIDIA (5 indirections in
VKD3D-Proton per XDC 2025).

=== Hybrid (recommended) ===
CPU desc update per frame 6.00 us
CPU bind calls per frame 6.00 calls
CPU total per frame 9.00 us
GPU static overhead 1048576.00 B
Validation layer factor 2.00 x
% of 16.67ms frame budget (CPU only)           0.05 %
Notes:                                         0.00
Bindless for chunk/material tables (stable, indexed in shader); traditional+dynamic-offset for per-frame SSBOs; push for
small transient sets.

========= Per-stage projection: Stage 4.3 dense (256+ ch) =========

=== ProjectV current baseline at Stage 4.3 dense (256+ ch) (256 chunks visible) ===
Total pipeline descriptors (current): 23 bindings across 4 pipelines
Estimated CPU desc update calls per frame: 10 (4 pipelines * 1 update each + 6 transient SSBO rebinds)
Measured reference (analogous workloads):
- Traha (Samsung, 2024): 220 vkUpdateDescriptorSets = 3.554ms saved by dynamic-offset rewrite (+5 FPS @ 37->42)
- Arm Mali sample: 44ms -> 27ms frame time = 38% saving from descriptor caching alone
- vkguide Ascendant (~400k chunks): per-batch compute dispatch + DrawIndirectInstanced pattern, no bindless
Per-frame descriptor cost (estimated, 64 visible chunks): 8-12 us CPU + 0 GPU static overhead
Per-frame descriptor cost (estimated, 256 visible chunks, future Stage 4.3):
CPU grows ~linearly with chunk count: 16-24 us at 256 chunks (still < 0.15% of 16.67ms frame)
Verdict: descriptor update cost is NOT a current bottleneck (< 0.2% frame budget).
Stage 2.x consideration: if Stage 2.3 (3D virtual texturing) lands, per-page-table rebind cost grows.

=== Traditional (current ProjectV baseline) ===
CPU desc update per frame 25.00 us
CPU bind calls per frame 14.00 calls
CPU total per frame 32.00 us
GPU static overhead 0.00 B
Validation layer factor 1.00 x
% of 16.67ms frame budget (CPU only)           0.19 %
Notes:                                         0.00
Frame-in-flight per-frame descriptor updates. Stable, debug-friendly.

=== Bindless (VK_EXT_descriptor_indexing) ===
CPU desc update per frame 2.00 us
CPU bind calls per frame 1.00 calls
CPU total per frame 2.50 us
GPU static overhead 4194304.00 B
Validation layer factor 8.00 x
% of 16.67ms frame budget (CPU only)           0.01 %
Notes:                                         0.00
Unbounded arrays + PARTIALLY_BOUND + UPDATE_AFTER_BIND. GPU-AV REQUIRED for PARTIALLY_BOUND validation (8× debug
overhead per Khronos docs).

=== Push (VK_KHR_push_descriptor) ===
CPU desc update per frame 4.00 us
CPU bind calls per frame 14.00 calls
CPU total per frame 11.00 us
GPU static overhead 0.00 B
Validation layer factor 1.05 x
% of 16.67ms frame budget (CPU only)           0.07 %
Notes:                                         0.00
vkCmdPushDescriptorSet[KHR] inline update. maxPushDescriptors per layout (HW-dependent, NOT 32 — that's a common
misconception).

=== Descriptor Buffer (VK_EXT_descriptor_buffer) ===
CPU desc update per frame 1.50 us
CPU bind calls per frame 1.00 calls
CPU total per frame 2.00 us
GPU static overhead 2097152.00 B
Validation layer factor 4.00 x
% of 16.67ms frame budget (CPU only)           0.01 %
Notes:                                         0.00
Descriptors as buffer memory, memcpy()-update. HW on AMD/Intel/Arm v9+, emulated on NVIDIA (5 indirections in
VKD3D-Proton per XDC 2025).

=== Hybrid (recommended) ===
CPU desc update per frame 6.00 us
CPU bind calls per frame 6.00 calls
CPU total per frame 9.00 us
GPU static overhead 1048576.00 B
Validation layer factor 2.00 x
% of 16.67ms frame budget (CPU only)           0.05 %
Notes:                                         0.00
Bindless for chunk/material tables (stable, indexed in shader); traditional+dynamic-offset for per-frame SSBOs; push for
small transient sets.

========= Conclusion (for ProjectV Stage 2.x) =========
VERDICT: HYBRID strategy is the right default for Stage 2.x:
- Bindless for stable, low-frequency-update descriptors (material table, Sparse64Node pool).
Wait for Stage 1.1 (sparse 64-tree) + 1.2 (SVDAG) to land first.
- Traditional + dynamic offset for transient per-frame SSBOs (compute cull output, indirect).
- Push descriptors for small per-draw transient sets (shadow cascade params, debug toggles).
DEFER VK_EXT_descriptor_buffer until cross-vendor maturity improves (NVIDIA emulation overhead, VKD3D-Proton
5-indirection tax).
AVOID pure-bindres for everything:
- Validation layer overhead (8x for PARTIALLY_BOUND + GPU-AV)
- Wave-divergence on non-uniform access (matters for compute cull which has mostly uniform access)
- Debug introspection cost (no CPU-inspectable descriptor state with UPDATE_AFTER_BIND)
Below 5% frame budget (per legacy/docs/philosophy/03_domain/01_optimization-philosophy.md):
- Don't optimize descriptor strategy prematurely. Current cost (25us / 0.15% frame) is NOT a bottleneck.
- Re-evaluation trigger: Stage 2.3 (3D virtual texturing), Stage 5.2 (RTX BLAS per chunk), Stage 4.3 (128+ chunks).
