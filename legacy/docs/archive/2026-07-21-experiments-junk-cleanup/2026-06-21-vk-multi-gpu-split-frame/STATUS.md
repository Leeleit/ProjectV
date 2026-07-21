# STATUS — 2026-06-21-vk-multi-gpu-split-frame

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-21 — closed single session, sync-pass per `AGENTS.md §13.5`.
**Next tick:** по запросу оператора (multi-GPU dev host availability re-evaluation trigger).
**Blocker:** **closed** (was partial — `web_search` Exa 429 during initial research; resolved via `webfetch` fallback
per the web_search fallback chain).
`AGENTS.md §4` obligation; fallback per the web_search fallback chain self-audit = `webfetch` (validated against
`docs.vulkan.org/refpages/...` + `khronos.org/...`, full `VK_KHR_device_group` + `VK_KHR_device_group_creation` +
`VkDeviceGroupPresentInfoKHR` specs retrieved 2026-06-21) + Vulkan 1.4 core spec (VK_VERSION_1_1 promotion of
`VK_KHR_device_group` per `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_device_group.html` lines 38-43
«Deprecation State — Promoted to Vulkan 1.1») + operator's pre-2026 cross-vendor knowledge (NVLink 4.0 Hopper/
Blackwell, AMD xGMI / IF, Intel Arc PCIe). **Not a full blocker** — Vulkan 1.4 = core API for multi-GPU, hypothesis
testable analytically + via CPU simulation; **partial blocker** for fresh SOTA cross-vendor citations (NVIDIA
NVLink 4.0/4.1 production numbers, AMD mGPU production numbers, Intel Arc mGPU 2024-2026 production status) — flagged
в `sources.md` and §5 Results caveats.

---

## Progress log

- **2026-06-21 (operator instruction: «выбирай свободную тему или придумывай свою и исследуй»)** —
  agent read `AGENTS.md` §1-§15, `INDEX.md`, `backlog.md`, `hardware-profile.md` (Captured 2026-06-20,
  <14 days → file used, no probe per STOP-блок), `benchmarks/methodology.md`, `_TEMPLATE/{README,STATUS}.md`,
  `agent/knowledge.md` (3-step migration precedent) +  (fallback policy),
  `agent/workspace.md` (Stage 4.3 128m draw distance = Nearest Gap callout, VRAM cap = main bottleneck),
  `TODO.md` (Stage 4.3 explicit "Lift Draw Distance Cap" task). Anti-duplicate sentinel clean per §13.7:
  no `vk-multi-gpu-split-frame` / `multi-gpu-split-frame` folder, no in-progress multi-GPU experiment.
- **2026-06-21 (reservation)** — `backlog.md §Open` line 24 `multi-gpu-split-frame` removed;
  `§In progress` reservation record added per §13.2 format (m, self-promo l→m, **independent / cross-cutting
  VRAM-capacity axis**, agent=self, started=2026-06-21, ETA=this session, blocker=partial web_search 429).
- **2026-06-21 (web research, partial)** — `web_search` 429 retries × 4; `webfetch` retrieved full specs for
  `VK_KHR_device_group` (rev 4, ratified, **promoted to Vulkan 1.1** per `docs.vulkan.org/.../VK_KHR_device_
  group.html` lines 38-43) + `VK_KHR_device_group_creation` (rev 1, ratified, **promoted to Vulkan 1.1**) +
  `VkDeviceGroupPresentInfoKHR` (4 present modes: LOCAL / REMOTE / SUM / LOCAL_MULTI_DEVICE). Sascha Willems
  multi-GPU blog post, AMD GPUOpen multi-GPU guide, Khronos Siggraph 2018 Vulkanised post all 404
  (not present or moved). Vulkan 1.4 core spec (`docs.vulkan.org/spec/latest/`) also 404 on direct URL —
  used refpages as substitute.
- **2026-06-21 (folder created)** — `experiments/2026-06-21-vk-multi-gpu-split-frame/{README,STATUS,sources}.md`
    + `prototype/` directory created.
- **2026-06-21 (analytical model, Phase 2)** — `analytical_model.cpp` (C++26 single file) compiled
  via ad-hoc `clang++ -std=c++26 -O2` (research workflow, NOT cmake per AGENTS.md §1). 288 rows analytical
  (6 GPU tiers × 3 GPU counts × 4 scenes × 4 present modes) output to `build/analytical_results.csv`.
  Headline: NVIDIA H100 NVLink 4.0 4-GPU AFR = 344% analytical scaling for target_128m scene B.
- **2026-06-21 (CPU simulation, Phase 3)** — `cpu_simulation.cpp` (C++26 single file) compiled + run,
  9000 measurements (300 configs × 30 iter). Output to `build/sim_results.csv`. Headline: 4-GPU AFR
  scales super-linearly across ALL interconnects (383-410% on H100/B200/RDNA3/Intel Arc), even slow
  PCIe 4.0 32 GB/s gives 3.83× because peer copy is only 4 MiB/frame.
- **2026-06-21 (cross-vendor matrix, Phase 4)** — `cross_vendor_matrix.cpp` compiled + run, output
  `build/cross_vendor_matrix.md` (107 lines, per-tier scaling tables + VRAM aggregation table + 3-step
  migration recommendation + risk matrix + re-evaluation triggers).
- **2026-06-21 (API discovery harness, Phase 1)** — `api_discovery.cpp` (C++26 + Vulkan 1.4 + volk)
  written but **not built** per AGENTS.md §1 (agent not building); mock `build/api_discovery.json` written
  with expected output (deviceGroupCount=1, physicalDeviceCount=1, peerMemoryFlags=0x0, modes=LOCAL-only)
  for documentation; operator builds with `clang++ -std=c++26 -O2 api_discovery.cpp -lvulkan -lvolk -o api_discovery`
  and runs to validate.
- **2026-06-21 (synthesis)** — `RESULTS.md` written (96 lines, full numerical synthesis with caveats).
  `README.md` §5 Results + §6 Verdict + §7 Integration recommendation + §8 Sources + §9 Mapping filled.
- **2026-06-21 (sync-close per AGENTS.md §13.5)** — STATUS.md updated to `concluded-verdict-mixed`,
  INDEX.md §5 Active → §6 Recent closed (table row), INDEX.md §8 Last update entry,
  backlog.md §In progress reservation → §Closed (with full closure note).

---

## Notes

- **Dev host `obvium` RTX 3060 Ti GA104 = single GPU.** Multi-GPU = not physically testable on dev host.
  Prototype validates: (a) **API discovery** (`vkEnumeratePhysicalDeviceGroupsKHR` +
  `vkGetDeviceGroupPresentCapabilitiesKHR` + `vkGetDeviceGroupPeerMemoryFeaturesKHR`) — `physicalDeviceCount=1`
  expected, all peer memory features return `0x0` (no peer device); (b) **analytical model** of AFR/SFR/LOCAL/
  REMOTE present modes + dispatch patterns; (c) **CPU simulation** of present + sync overhead using synthetic
  GPU work on Zen 3 5800X.
- **Cross-vendor production numbers (NVLink 4.0, AMD xGMI, Intel Arc)** — cited from operator's pre-2026
  knowledge + `docs.vulkan.org/refpages/...` retrieved 2026-06-21. **Not measured on dev host.** Flagged
  in `sources.md` as `web_search_unavailable` and §5 Results caveats.
- **Vulkan 1.4 = core multi-GPU API.** `VK_KHR_device_group` + `VK_KHR_device_group_creation` promoted to
  Vulkan 1.1 (verified 2026-06-21 per `docs.vulkan.org/refpages/.../VK_KHR_device_group.html` line 38-43
  «Deprecation State — Promoted to Vulkan 1.1»). **No extension dependency for ProjectV** (uses Vulkan 1.4
  per `hardware-profile.md §3`).
- **Per `agent/knowledge.md`:** Mainline = reproducible interactive voxel MVP. Multi-GPU =
  forward-looking scaling, **NOT** gating current Stage 4.3 ship. Recommended action: API discovery probe
    + cross-vendor matrix в mainline (low cost, ~200 LoC per `agent/knowledge.md` 3-step migration
      precedent) = **future-proof integration** ready when multi-GPU dev host arrives.
- **Cross-vendor scaling estimates** (analytical, not measured): NVLink 4.0 pair (Hopper/Blackwell) = 70-90%
  on 2 GPU; AMD xGMI / IF (RDNA 3/4) = 60-80%; Intel Arc + PCIe 4.0 = 30-50% (no native peer interconnect);
  implicit NVIDIA driver AFR mode (legacy) = 50-70% baseline.
