#include "ui/HudStyle.hpp"

#include "imgui.h"

namespace projectv::ui {

void ApplyGameDevHudStyle()
{
	ImGui::StyleColorsDark();
	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding = 8.0f;
	style.ChildRounding = 6.0f;
	style.FrameRounding = 5.0f;
	style.PopupRounding = 6.0f;
	style.ScrollbarRounding = 6.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 5.0f;
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.WindowPadding = ImVec2(10.0f, 8.0f);
	style.FramePadding = ImVec2(8.0f, 4.0f);
	style.ItemSpacing = ImVec2(8.0f, 6.0f);
	style.IndentSpacing = 14.0f;

	ImVec4 *colors = style.Colors;
	const ImVec4 bg{0.10f, 0.11f, 0.14f, 0.94f};
	const ImVec4 bgSoft{0.14f, 0.16f, 0.20f, 1.00f};
	const ImVec4 accent{0.20f, 0.78f, 0.72f, 1.00f};
	const ImVec4 accentHover{0.30f, 0.88f, 0.82f, 1.00f};
	const ImVec4 accentActive{0.12f, 0.62f, 0.58f, 1.00f};
	const ImVec4 text{0.92f, 0.94f, 0.96f, 1.00f};

	colors[ImGuiCol_Text] = text;
	colors[ImGuiCol_WindowBg] = bg;
	colors[ImGuiCol_ChildBg] = ImVec4(bg.x, bg.y, bg.z, 0.70f);
	colors[ImGuiCol_PopupBg] = bg;
	colors[ImGuiCol_Border] = ImVec4(0.22f, 0.26f, 0.32f, 0.70f);
	colors[ImGuiCol_FrameBg] = bgSoft;
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.26f, 0.32f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.14f, 0.16f, 1.00f);
	colors[ImGuiCol_CheckMark] = accent;
	colors[ImGuiCol_SliderGrab] = accent;
	colors[ImGuiCol_SliderGrabActive] = accentActive;
	colors[ImGuiCol_Button] = ImVec4(0.16f, 0.22f, 0.26f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.32f, 0.34f, 1.00f);
	colors[ImGuiCol_ButtonActive] = accentActive;
	colors[ImGuiCol_Header] = ImVec4(0.14f, 0.24f, 0.26f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.34f, 0.36f, 1.00f);
	colors[ImGuiCol_HeaderActive] = accentActive;
	colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.30f, 0.34f, 0.60f);
	colors[ImGuiCol_Tab] = bgSoft;
	colors[ImGuiCol_TabHovered] = accentHover;
	colors[ImGuiCol_TabSelected] = accentActive;
	colors[ImGuiCol_TabSelectedOverline] = accent;
}

} // namespace projectv::ui
