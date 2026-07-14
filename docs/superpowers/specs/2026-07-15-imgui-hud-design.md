# ImGui HUD + remapped input

Date: 2026-07-15

## Summary

Replace bitmap DebugHud with Dear ImGui: always-on status strip, Settings window
(`` ` ``), optional Stats panel. Keyboard limited to movement/look + F1/`/Tab/Esc;
rare debug actions live in Settings checkboxes/buttons.

## Architecture

- `ImGuiLayer`: SDL3 + Vulkan dynamic rendering (volk), swapchain recreate hook.
- `HudPanels` / `HudStyle`: game-dev theme strip + Settings + Stats.
- Draw in post-AA swapchain UI pass via `ImGuiRenderDrawData`.
- `WantCapture*` gates game input; Open Settings frees relative mouse.

## Status

Implemented on `main` (see `agent/knowledge.md` §28).
