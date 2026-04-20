#include "imgui.h"

extern "C" void dusk_mod_set_imgui_ctx(void* ctx) {
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx));
}
