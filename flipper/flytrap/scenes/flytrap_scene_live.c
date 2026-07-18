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

// "Install firmware" on the no-board prompt.
static void flytrap_noboard_button_cb(GuiButtonType result, InputType type, void* context) {
    FlytrapApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter)
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventInstallFirmware);
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

// A centered status card (title + divider + two lines). Used for the detecting /
// no-board / starting screens so a blocking step never shows a blank frame.
static void flytrap_status_screen(FlytrapApp* app, const char* l1, const char* l2) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Flytrap");
    widget_add_line_element(app->widget, 0, 20, 127, 20);
    widget_add_string_element(app->widget, 64, 27, AlignCenter, AlignTop, FontSecondary, l1);
    if(l2 && l2[0])
        widget_add_string_element(app->widget, 64, 42, AlignCenter, AlignTop, FontSecondary, l2);
}

// Render the screen for the current start state: detecting / no-board / starting,
// then the dashboard once broadcasting (or on error / a lost board link).
static void flytrap_live_render(FlytrapApp* app) {
    const char* s = furi_string_get_cstr(app->status);
    if(strcmp(s, "detecting") == 0) {
        flytrap_status_screen(app, "Detecting board...", "");
        return;
    }
    if(strcmp(s, "noboard") == 0) {
        // No Flytrap beacon: board off, or it needs Flytrap firmware. Offer to
        // install it (and keep auto-detecting in case a good board appears).
        flytrap_status_screen(app, "No Flytrap board", "Attach a board, or:");
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Install fw", flytrap_noboard_button_cb, app);
        return;
    }
    bool live = false;
    flytrap_state_label(s, &live);
    bool err = strstr(s, "err") != NULL;
    if(live || err || app->link_lost) {
        flytrap_dash_refresh(app);
    } else {
        FuriString* tmp = furi_string_alloc();
        furi_string_printf(tmp, "SSID: %s", furi_string_get_cstr(app->ssid));
        flytrap_status_screen(app, "Starting portal...", furi_string_get_cstr(tmp));
        furi_string_free(tmp);
    }
}

void flytrap_scene_live_on_enter(void* context) {
    FlytrapApp* app = context;
    if(!app->session_active) {
        // Fresh start: paint "Detecting board..." first, then run the (possibly
        // blocking) check on the next loop pass so it doesn't freeze on a blank frame.
        app->awaiting_board = false;
        app->link_lost = false; // clear any stale disconnect from a prior session
        furi_string_set(app->status, "detecting");
        flytrap_live_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventDetectBoard);
    } else {
        flytrap_live_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
    }
}

bool flytrap_scene_live_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case FlytrapEventDetectBoard:
        if(flytrap_board_present(app, 2500)) {
            furi_string_set(app->status, "starting");
            flytrap_live_render(app); // paint "Starting..." before the blocking send
            view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventBeginSend);
        } else {
            // Keep watching: the tick resumes this flow when the board's beacon
            // shows up, so plugging it in auto-continues without a Back + Start.
            app->awaiting_board = true;
            furi_string_set(app->status, "noboard");
            flytrap_live_render(app);
        }
        return true;
    case FlytrapEventBeginSend:
        flytrap_session_start(app); // blocks while the portal streams; "Starting..." stays
        flytrap_live_render(app);
        return true;
    case FlytrapEventInstallFirmware:
        // Go flash the bundled firmware; on return this scene re-detects and, with a
        // now-flashed board, continues the start. Stop watching while we're away.
        app->awaiting_board = false;
        furi_string_set(app->flash_manifest, FLYTRAP_DEFAULT_FW);
        scene_manager_next_scene(app->scene_manager, FlytrapSceneFlash);
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
    FlytrapApp* app = context;
    app->awaiting_board = false; // stop watching once we leave the start screen
    // Session persists across the menu/sub-views; stopped via menu "Stop" or app exit.
}
