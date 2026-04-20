// Tests every feature of the Dusk mod API. Results shown in the mod tab.

#include "d/actor/d_a_alink.h"
#include "dusk/hook.hpp"
#include "dusk/mod_api.h"
#include "imgui.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdio>
#include <string>

static int  g_ticks            = 0;
static bool g_pre_fired        = false;
static bool g_post_fired       = false;
static bool g_replace_fired    = false;
static bool g_arg_write_ok     = false;
static int  g_pre_cancel_count = 0;
static int  g_post_count       = 0;
static bool g_resource_ok      = false;
static std::string g_resource_text;
static char g_mod_dir_snippet[64] = {};

// Pre-hook on posMove. Hold L to test argRef write and cancellation.
static int32_t on_posMove_pre(void* args) {
    g_pre_fired = true;
    if (mDoCPd_c::getHoldL(PAD_1)) {
        dusk::argRef<daAlink_c*>(args, 0)->shape_angle.y = 0;
        g_arg_write_ok = true;
        ++g_pre_cancel_count;
        return 1; // cancel
    }
    return 0;
}

// Post-hook on posMove. Fires even when the pre-hook cancelled.
static void on_posMove_post(void* args) {
    g_post_fired = true;
    ++g_post_count;
    (void)args;
}

// Replace-hook on execute. Calls through to the original so gameplay is unaffected.
using ExecuteEntry = dusk::HookEntry<&daAlink_c::execute>;
static void on_execute_replace(void* args) {
    g_replace_fired = true;
    ExecuteEntry::g_orig(dusk::arg<daAlink_c*>(args, 0));
}

static void DrawPanel(void*) {
    auto status = [](const char* label, bool ok) {
        ImGui::TextColored(ok ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.35f, 0.35f, 1),
                           ok ? "[PASS]" : "[WAIT]");
        ImGui::SameLine();
        ImGui::Text("%s", label);
    };

    ImGui::SeparatorText("Hooks");
    status("pre-hook fired (posMove)", g_pre_fired);
    status("post-hook fired (posMove)", g_post_fired);
    status("replace-hook fired (execute)", g_replace_fired);
    status("argRef write + pre cancel (hold L)", g_arg_write_ok);
    ImGui::Text("  pre cancels: %d   post calls: %d", g_pre_cancel_count, g_post_count);

    ImGui::SeparatorText("Resources");
    status("load_resource (hello.txt)", g_resource_ok);
    if (!g_resource_text.empty())
        ImGui::TextWrapped("  \"%s\"", g_resource_text.c_str());

    ImGui::SeparatorText("API Fields");
    status("mod_dir non-empty", g_mod_dir_snippet[0] != '\0');
    ImGui::TextWrapped("  %s", g_mod_dir_snippet);

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Reset results")) {
        g_pre_fired        = false;
        g_post_fired       = false;
        g_replace_fired    = false;
        g_arg_write_ok     = false;
        g_pre_cancel_count = 0;
        g_post_count       = 0;
    }
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link) {
        ImGui::SameLine();
        ImGui::Text("(Link y angle: %d)", (int)link->shape_angle.y);
    }
}

static void DrawMenuEntry(void*) {
    if (ImGui::MenuItem("Test: log all levels")) {
        dusk::g_api->log_info("log_info test");
        dusk::g_api->log_warn("log_warn test");
        dusk::g_api->log_error("log_error test");
    }
    if (ImGui::MenuItem("Test: reset Link y angle")) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link) link->shape_angle.y = 0;
    }
}

extern "C" {

void mod_init(DuskModAPI* api) {
    dusk::init(api);

    api->log_info("mod_test initializing");
    api->log_warn("log_warn smoke test");
    api->log_error("log_error smoke test");

    std::snprintf(g_mod_dir_snippet, sizeof(g_mod_dir_snippet), "%.60s", api->mod_dir);

    size_t size = 0;
    void* data = api->load_resource("hello.txt", &size);
    if (data) {
        g_resource_text.assign(static_cast<char*>(data), size);
        while (!g_resource_text.empty() && g_resource_text.back() == '\n')
            g_resource_text.pop_back();
        api->free_resource(data);
        g_resource_ok = true;
        api->log_info("load_resource OK: \"%s\"", g_resource_text.c_str());
    } else {
        api->log_error("load_resource FAILED for hello.txt");
    }

    // Missing file should return nullptr gracefully.
    void* missing = api->load_resource("does_not_exist.bin", nullptr);
    if (!missing)
        api->log_info("load_resource missing-file: correctly returned nullptr");
    else {
        api->log_error("load_resource missing-file: unexpectedly returned data");
        api->free_resource(missing);
    }

    dusk::hookAddPre <&daAlink_c::posMove>(on_posMove_pre);
    dusk::hookAddPost<&daAlink_c::posMove>(on_posMove_post);
    dusk::hookSetReplace<&daAlink_c::execute>(on_execute_replace);

    api->register_tab_content(DrawPanel, nullptr);
    api->register_menu_item(DrawMenuEntry, nullptr);

    api->log_info("mod_test ready");
}

void mod_tick(DuskModAPI* api) {
    ++g_ticks;
    (void)api;
}

void mod_cleanup(DuskModAPI* api) {
    api->log_info("mod_test unloaded after %d ticks", g_ticks);
}

}
