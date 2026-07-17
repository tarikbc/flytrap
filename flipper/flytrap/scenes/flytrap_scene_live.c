#include "../flytrap_i.h"
#include "../helpers/flytrap_session.h"

static void flytrap_dash_button_cb(GuiButtonType result, InputType type, void* context) {
    FlytrapApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventShowCaptures);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventShowClients);
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
    // A lost board link overrides whatever the last status token said.
    bool live = false;
    const char* state;
    if(app->link_lost) {
        state = "Board disconnected";
        live = false;
    } else {
        state = flytrap_state_label(furi_string_get_cstr(app->status), &live);
    }
    widget_add_circle_element(app->widget, 4, 31, 3, live);
    widget_add_string_element(app->widget, 12, 28, AlignLeft, AlignTop, FontSecondary, state);

    // Counters.
    furi_string_printf(tmp, "Creds: %u   Clients: %u", app->cap_count, app->client_count);
    widget_add_string_element(
        app->widget, 0, 41, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    // The two live views on the left/right; Console lives in the main menu.
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Captures", flytrap_dash_button_cb, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Clients", flytrap_dash_button_cb, app);

    furi_string_free(tmp);
}

// Shown while the portal is coming up — the ESP handshake plus the ~3s it takes
// to stream the portal HTML over UART (which blocks the UI), so the user sees
// progress instead of a frozen dashboard.
static void flytrap_loading_render(FlytrapApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Flytrap");
    widget_add_line_element(app->widget, 0, 20, 127, 20);
    widget_add_string_element(
        app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Starting portal...");
    FuriString* tmp = furi_string_alloc();
    furi_string_printf(tmp, "SSID: %s", furi_string_get_cstr(app->ssid));
    widget_add_string_element(
        app->widget, 64, 38, AlignCenter, AlignTop, FontSecondary, furi_string_get_cstr(tmp));
    widget_add_string_element(
        app->widget, 64, 51, AlignCenter, AlignTop, FontSecondary, "Bringing up Wi-Fi");
    furi_string_free(tmp);
}

// Loading screen until the portal is broadcasting (or errors), then the dashboard.
static void flytrap_live_render(FlytrapApp* app) {
    bool live = false;
    flytrap_state_label(furi_string_get_cstr(app->status), &live);
    bool err = strstr(furi_string_get_cstr(app->status), "err") != NULL;
    if(live || err || app->link_lost) {
        flytrap_dash_refresh(app);
    } else {
        flytrap_loading_render(app);
    }
}

void flytrap_scene_live_on_enter(void* context) {
    FlytrapApp* app = context;
    if(!app->session_active) {
        // Fresh start: paint the loading screen first, then kick off the blocking
        // send on the next event loop pass so it doesn't freeze on a blank frame.
        furi_string_set(app->status, "starting");
        flytrap_loading_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventBeginSend);
    } else {
        flytrap_live_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
    }
}

bool flytrap_scene_live_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case FlytrapEventBeginSend:
        flytrap_session_start(app); // blocks while the portal streams; loading stays up
        flytrap_live_render(app);
        return true;
    case FlytrapEventRefreshView:
        flytrap_live_render(app);
        return true;
    case FlytrapEventShowCaptures:
        scene_manager_next_scene(app->scene_manager, FlytrapSceneCapturesList);
        return true;
    case FlytrapEventShowClients:
        scene_manager_next_scene(app->scene_manager, FlytrapSceneClientsList);
        return true;
    default:
        return false;
    }
}

void flytrap_scene_live_on_exit(void* context) {
    UNUSED(context);
    // Session persists across the menu/sub-views; stopped via menu "Stop" or app exit.
}
