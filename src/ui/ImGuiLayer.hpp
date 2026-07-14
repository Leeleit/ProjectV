#pragma once

#include "core/Types.hpp"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

namespace projectv::ui {

[[nodiscard]] bool InitImGuiLayer(AppState &state);
void ShutdownImGuiLayer(AppState &state);
void OnImGuiSwapchainRecreated(AppState &state);
void ImGuiProcessEvent(const SDL_Event *event);
void ImGuiNewFrame();
void ImGuiRenderDrawData(VkCommandBuffer cmd);
[[nodiscard]] bool ImGuiWantCaptureKeyboard();
[[nodiscard]] bool ImGuiWantCaptureMouse();
[[nodiscard]] bool IsImGuiLayerReady();
void ImGuiEndFrameIfOpen();

} // namespace projectv::ui
