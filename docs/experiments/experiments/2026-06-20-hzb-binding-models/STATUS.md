# STATUS — 2026-06-20-hzb-binding-models

**Phase:** wrap-up
**Last action:** 2026-06-20 — prototype complete, sampling correctness measured, RESULTS.md written.
**Next tick:** verdict integration в mainline `bindless-descriptor-overhead` (Phase E update + Stage 2.2
integration notes).
**Blocker:** нет.

---

## Progress log

- **2026-06-20 — открыт.** Резервирование в `backlog.md §In progress` per §13.2. INDEX.md §5 обновлён.
  Prior art собран (~10 источников). Hypothesis H1/H2/H3 сформулированы.
- **2026-06-20 — prototype build complete.** Standalone Vulkan compute harness с 3 sampling shaders
  (`textureLod`, `texelFetch_storage_image`, `texelFetch_sampled_image`). Сборка green, исполнение OK.
- **2026-06-20 — measurement complete.** 17/24 PASS, 7/24 FAIL. Storage image binding **fundamentally
  unsuited** для HZB (single mip per descriptor). `textureLod` + `texelFetch(sampled)` обе работают на
  classic descriptor sets. `foijord` NVIDIA bug на bindless — **литературное подтверждение, не
  воспроизведено на dev host** (требует `VK_KHR_maintenance5` + `VkPhysicalDeviceDescriptorHeapFeaturesEXT`).
- **2026-06-20 — results written.** `prototype/RESULTS.md` + `prototype/results.csv` (machine-readable).
  4 sec §1 test matrix + raw data + analysis + conclusion + mapping to ProjectV hot-path.

---

## Notes

- **Critical finding (P0):** storage image (`VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` + `imageLoad`) **не может
  sample разные mips** через один descriptor — fundamental GLSL/Vulkan limitation. Это автоматически
  отвергает storage image binding model для HZB culling (где mip выбирается per-chunk динамически).
- **Critical finding (P1):** `texelFetch(sampler2D, ivec2, mipLevel)` works correctly across all 8 mips,
  на classic descriptor sets, на dev host. Robust по литературным данным под bindless heap
  (`foijord/vk-textureLod-repro` 2026).
- **Critical finding (P2):** `textureLod(sampler2D, uv, mipLevel)` works на dev host classic, но **fragile**
  под bindless heap на NVIDIA (foijord repro). Не критично для текущего `HizCulling.cpp` (classic path),
  но если Stage 2.2 wire-up + `bindless-descriptor-overhead` Phase E rollout объединяются — `textureLod`
  pattern становится risk surface.
- **Cross-axis continuity:** same-day `2026-06-20` сессии закрыли 6 storage/cull/bindless experiments; этот
  завершает **sync-axis + binding-axis** исследования для Stage 2.2 + Phase E.
- **Out of scope (per AGENTS.md §2):** Stage 2.2 integration в mainline Renderer.cpp — это **задача
  mainline-агента**, не research. Я передаю **integration recommendation** через §7 README + cross-ref
  в `bindless-descriptor-overhead`.
- **Cross-protocol:** параллельная сессия `2026-06-20-nanovdb-on-gpu` (SVO storage alternative) — non-overlapping
  scope, нет conflict.

---

## Next session triggers

- (none expected) — verdict `mixed` (recommend texelFetch, with caveats), mainline integration is sequential
  Stage 2.2 work, не research frontier.
- Если mainline приземляет Phase E (bindless HZB) и сталкивается с `textureLod` failure — re-open
  `hzb-binding-models` с explicit bindless heap test (добавить `VK_KHR_maintenance5` + heap test).
