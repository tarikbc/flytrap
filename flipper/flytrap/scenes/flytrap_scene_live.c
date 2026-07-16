#include "../flytrap_i.h"

static void flytrap_dash_button_cb(GuiButtonType result, InputType type, void* context) {
    FlytrapApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventShowCaptures);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventShowConsole);
    }
}

// Map the raw ESP status token to a polished label; sets *live when broadcasting.
static const char* flytrap_state_label(const char* raw, bool* live) {
    *live = false;
    if(strncmp(raw, "portal_up", 9) == 0) {
        *live = true;
        return "Broadcasting";
    }
    if(strstr(raw, "err") != NULL) return "Portal file error";
    if(strcmp(raw, "stopped") == 0) return "Stopped";
    if(strcmp(raw, "idle") == 0) return "Idle";
    if(strcmp(raw, "starting") == 0 || strncmp(raw, "html", 4) == 0 ||
       strncmp(raw, "ap_ok", 5) == 0)
        return "Starting...";
    return raw;
}

static void flytrap_dash_refresh(FlytrapApp* app) {
    widget_reset(app->widget);
    FuriString* tmp = furi_string_alloc();

    // Title + divider.
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Flytrap");
    widget_add_line_element(app->widget, 0, 14, 127, 14);

    // SSID.
    furi_string_printf(tmp, "AP: %s", furi_string_get_cstr(app->ssid));
    widget_add_string_element(
        app->widget, 0, 18, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    // Status: a filled dot when broadcasting (ring otherwise) + a polished label.
    bool live = false;
    const char* state = flytrap_state_label(furi_string_get_cstr(app->status), &live);
    widget_add_circle_element(app->widget, 4, 31, 3, live);
    widget_add_string_element(app->widget, 12, 28, AlignLeft, AlignTop, FontSecondary, state);

    // Counters.
    furi_string_printf(tmp, "Creds: %u   Clients: %u", app->cap_count, app->client_count);
    widget_add_string_element(
        app->widget, 0, 41, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    // Buttons on the left and right.
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Captures", flytrap_dash_button_cb, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Console", flytrap_dash_button_cb, app);

    furi_string_free(tmp);
}

void flytrap_scene_live_on_enter(void* context) {
    FlytrapApp* app = context;
    // The session is started by the main menu; this scene only renders it.
    flytrap_dash_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
}

bool flytrap_scene_live_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case FlytrapEventRefreshView:
        flytrap_dash_refresh(app);
        return true;
    case FlytrapEventShowCaptures:
        scene_manager_next_scene(app->scene_manager, FlytrapSceneCapturesList);
        return true;
    case FlytrapEventShowConsole:
        app->textview_mode = TextViewConsole;
        scene_manager_next_scene(app->scene_manager, FlytrapSceneTextView);
        return true;
    default:
        return false;
    }
}

void flytrap_scene_live_on_exit(void* context) {
    UNUSED(context);
    // Session persists across the menu/sub-views; stopped via menu "Stop" or app exit.
}
