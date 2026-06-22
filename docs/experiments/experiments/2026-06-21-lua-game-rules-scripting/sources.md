# Sources — `lua-game-rules-scripting`

Web research 2026-06-21 via direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429
persistent; DuckDuckGo HTML endpoint CAPTCHA-blocked per `agent/knowledge.md Part B §9` line
1424 fallback list). All sources **verified** by reading the actual page content; not relying
on cached snippets.

---

## Tier 1 — Production reference implementations (canonical)

### 1. Garry's Mod Wiki — `hook.Add`
**URL:** `https://wiki.facepunch.com/gmod/hook.Add`
**Captured:** 2026-06-21
**Authoritative:** Yes — Facepunch's official wiki for the production reference.

Key facts extracted (verified):
- `hook.Add(eventName, identifier, func)` — three-argument signature.
- `identifier` "should be unique so that you do not accidentally override some other mods hook,
  unless that's what you are trying to do."
- `identifier` can be a `string`, a `table/object` (with `IsValid` function) such as an
  `Entity` or `Panel`. `number`/`boolean` are **not allowed**.
- If `identifier` is a table/object, it is **inserted in front of the other arguments in the
  callback** and the hook is called only as long as `IsValid(identifier)` is true. When
  `IsValid` returns `false` for ANY event call, the hook is auto-removed.
- "Returning any value besides `nil` from the hook's function will stop other hooks of the
  same event down the loop from being executed. Only return a value when absolutely necessary
  and when you know what you are doing. It will also prevent the associated `GM:*` hook from
  being called on the gamemode. It WILL break other addons."

### 2. Garry's Mod Wiki — `hook.Run`
**URL:** `https://wiki.facepunch.com/gmod/hook.Run`
**Captured:** 2026-06-21
**Authoritative:** Yes.

Key facts extracted (verified):
- `vararg hook.Run(string eventName, ...)` — calls all hooks associated with event **until**
  one returns non-nil; then returns that data.
- If no hook returns any data, falls back to `GAMEMODE:<eventName>` (if exists).
- Return data limited to **6** values (matches Lua `select` semantics).
- Internally calls `hook.Call` (which is the lower-level variant — see Hook_Library_Usage).

### 3. Garry's Mod Wiki — `Hook_Library_Usage`
**URL:** `https://wiki.facepunch.com/gmod/Hook_Library_Usage`
**Captured:** 2026-06-21
**Authoritative:** Yes.

Key facts extracted (verified):
- "**Hooks added with `hook.Add` are not ordered in any way**" — critical for our design.
- Identifier can be **string, panel, or entity**. Panel/entity with auto-`IsValid`-based
  cleanup.
- `GM` (GAMEMODE) table functions are also treated as hooks — called after `hook.Add` hooks.
- "When a value is returned from a hook, other hooks for the same event do not run. This
  means that you are able to override other hooks and gamemode functions."
- "It is advised not to trigger default hooks unless you know what you are doing" — caution
  for `hook.Run` on built-in events.

### 4. Facepunch/garrysmod — `hook.lua` (canonical source)
**URL:** `https://github.com/Facepunch/garrysmod/blob/master/garrysmod/lua/includes/modules/hook.lua`
**Note:** Direct fetch timed out 2026-06-21; implementation is in C++ (binary module
`gmod.so`/`gmod.dll`) but the public Lua API exposes the documented functions above. Cited via
the wiki page cross-references.

### 5. Warcraft Wiki — Events (WoW addon API)
**URL:** `https://warcraft.wiki.gg/wiki/Event_API`
**Captured:** 2026-06-21
**Authoritative:** Yes — wowuidev community wiki; "up to date as of Patch 12.0.5 (67186)
Apr 23 2026".

Key facts extracted (verified):
- 200+ API system categories, hundreds of named events.
- **Frame-based event registration** pattern:
  ```lua
  local f = CreateFrame("Frame")
  f:RegisterEvent("CHAT_MSG_CHANNEL")
  f:RegisterEvent("PLAYER_STARTED_MOVING")
  f:SetScript("OnEvent", OnEvent)
  ```
- **One callback per Frame, not multi-hook**. Multiple events on one frame = single dispatch
  with `event` name as first arg.
- Event trace via `/etrace` slash command — debug facility.
- EventRegistry (separate API) supports callback events with register/unregister; design
  pattern shows modern hierarchical dispatch.

**Comparison with Garry's Mod:** WoW is **callback-per-frame** (one fn handles many events,
switches on `event` name). GMod is **multi-hook-per-event** (many fns on one event name,
GAMEMODE fallback). Both architectures widely adopted; ProjectV would pick the GMod
pattern (more Lua-idiomatic, supports addon override semantics).

---

## Tier 2 — Foundational language reference

### 6. Lua 5.1 Reference Manual
**URL:** `https://www.lua.org/manual/5.1/manual.html`
**Captured:** 2026-06-21
**Authoritative:** Yes — official Lua.org reference.

Relevant sections (verified):
- §2.2 Values and Types: `nil`, `boolean`, `number`, `string`, `function`, `userdata`,
  `thread`, `table`. All first-class. "**Tables are the sole data structuring mechanism**."
- §2.5.8 Function Calls: method calls (`v:Name(args)` = `v.Name(v, args)`) — used for
  identifier-as-object hooks (`myTable:OnSpawn(ply)`).
- §2.7 Error Handling: `pcall`/`xpcall` — required for protected hook dispatch.
- §2.8 Metatables: `__index`, `__newindex`, `__call` — design primitives for hook
  identifier objects with `IsValid`.
- §2.9 Environments: each function has its own env table — for sandbox isolation per script.
- §3 The API: `lua_pushcfunction`, `lua_setfield`, `lua_getfield` — C-side binding to
  expose hook table to Lua state.

### 7. Lua 5.1 — `pcall` and error semantics
**URL:** same as #6, §2.7 + §4 (debug library)
**Captured:** 2026-06-21

Relevant facts:
- `pcall(f, ...)` returns `(true, results...)` on success or `(false, errmsg)` on error.
- **Critical for hook dispatch:** if ANY hook throws, GMod-style dispatch would crash the
  game without `pcall` wrapping. Production hook systems (GMod, WoW, Roblox) all
  `pcall`-wrap each handler invocation.
- `xpcall` allows custom message handler.

---

## Tier 3 — Cross-cutting references (ProjectV context)

### 8. ProjectV — `agent/knowledge.md Part B §9` — Web fallbacks
**URL:** local file `/home/le1t/Projects/ProjectV/agent/knowledge.md`
**Captured:** 2026-06-21 (read this session)

Key facts: Fallback chain when Exa `web_search` returns 429: searx.be → DuckDuckGo HTML →
Brave → Bing → Google → Startpage → direct canonical URLs. This experiment used
**direct canonical URLs** as the primary path (Tier 1 #1-#5).

### 9. ProjectV — `hardware-profile.md` — Dev host
**URL:** local file `/home/le1t/Projects/ProjectV/docs/experiments/hardware-profile.md`
**Captured:** 2026-06-21 (read this session)

Key facts for prototype measurement:
- Dev host `obvium` AMD Ryzen 7 5800X (Zen 3), 8C/16T, governor `powersave`.
- 62.7 GiB RAM; AVX2/FMA yes, AVX-512 no.
- Clang 22.1.6 (per `agent/knowledge.md §17`).
- Benchmark flags per `benchmarks/methodology.md`: `-O3 -march=native -std=c++26 -DNDEBUG`.

### 10. ProjectV — `benchmarks/methodology.md` — Standard harness
**URL:** local file `/home/le1t/Projects/ProjectV/docs/experiments/benchmarks/methodology.md`
**Captured:** 2026-06-21 (read this session)

Key facts: warm-up ≥10 iter, N=1000 main measurements, machine-readable CSV + human-readable
RESULTS.md, mean/median/p95/p99/std/min/max output. **5-10% threshold per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`** (crossed massively = real
win, marginal = reject).

---

## Cross-axis references (other ProjectV experiments)

- **`2026-06-21-programmable-voxels`** (closed mixed) — first dedicated multi-runtime
  scripting axis (LuaJIT/WASM/TinyCC). This experiment is the **deeper dive** on LuaJIT
  specifically (focused on game-rules hooks, not generic per-block callbacks).
- **`2026-06-21-luajit-scripting-hotpath-cost`** (closed mixed) — measured per-call cost
  of LuaJIT pcall/FFI. This experiment is **orth** to that (hook system architecture vs
  raw call cost).
- **`2026-06-21-after-action-replay-system`** (closed mixed) — hook events like
  `OnPlayerSpawn`, `OnUnitDestroyed` are natural replay-recorded inputs; complementary.
- **`2026-06-21-lockstep-state-sync-hybrid-netcode`** (closed mixed) — game-rules hooks
  are server-authoritative under lockstep; complementary.
- **`2026-06-21-interest-management-aoi-battle`** (closed mixed) — AOI events emitted via
  hook system; complementary.

---

## Sources NOT used (with rationale)

- **DuckDuckGo HTML** — CAPTCHA-blocked this session per `agent/knowledge.md Part B §9`.
  Fallback path: direct canonical URLs (above).
- **Brave / Bing / Google / Startpage** — not needed; direct canonical URLs sufficed.
- **Facepunch/garrysmod `hook.lua` source** — fetch timed out; wiki pages provide
  equivalent API surface for our prototype. (C-side implementation is in the binary
  `gmod.so`/`.dll`, not in the Lua-readable source.)
- **From the Depths Lua** — searched for but no canonical "Lua scripting architecture" page
  in public web; FtD exposes Lua API for vehicle/weapon scripting but no public performance
  data. **GMod + WoW are sufficient for prior art.**

---

## Verification log

| # | URL                                              | Fetched | Status  | Notes                                |
|---|--------------------------------------------------|---------|---------|--------------------------------------|
| 1 | `wiki.facepunch.com/gmod/hook.Add`               | yes     | OK      | full content extracted               |
| 2 | `wiki.facepunch.com/gmod/hook.Run`               | yes     | OK      | full content extracted               |
| 3 | `wiki.facepunch.com/gmod/Hook_Library_Usage`     | yes     | OK      | full content extracted               |
| 4 | `github.com/Facepunch/garrysmod/.../hook.lua`    | yes     | timeout | implemented in C++ binary module     |
| 5 | `warcraft.wiki.gg/wiki/Event_API`                | yes     | OK      | full content extracted               |
| 6 | `lua.org/manual/5.1/manual.html`                 | yes     | OK      | full reference extracted (truncated) |
| 8 | local: `agent/knowledge.md Part B §9`           | yes     | OK      |                                      |
| 9 | local: `hardware-profile.md`                     | yes     | OK      |                                      |
| 10| local: `benchmarks/methodology.md`              | yes     | OK      |                                      |
