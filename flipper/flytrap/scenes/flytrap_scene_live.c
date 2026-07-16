#include "../flytrap_i.h"

static void flytrap_dash_button_cb(GuiButtonType result, InputType type, void* context) {
    FlytrapApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventShowCaptures);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventShowConsole);
    }
}

static void flytrap_dash_refresh(FlytrapApp* app) {
    widget_reset(app->widget);
    FuriString* tmp = furi_string_alloc();

    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Flytrap Portal");

    furi_string_printf(tmp, "AP: %s", furi_string_get_cstr(app->ssid));
    widget_add_string_element(
        app->widget, 0, 15, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    furi_string_printf(tmp, "State: %s", furi_string_get_cstr(app->status));
    widget_add_string_element(
        app->widget, 0, 25, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    furi_string_printf(tmp, "Clients: %u   Creds: %u", app->client_count, app->cap_count);
    widget_add_string_element(
        app->widget, 0, 35, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    if(app->cap_count > 0) {
        uint8_t idx = (app->cap_head + FLYTRAP_CAP_SLOTS - 1) % FLYTRAP_CAP_SLOTS;
        furi_string_printf(tmp, "Last: %s", app->captures[idx].kv);
        widget_add_string_element(
            app->widget, 0, 45, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));
    }

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Captures", flytrap_dash_button_cb, app);
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
        app->textview_mode = TextViewCaptures;
        scene_manager_next_scene(app->scene_manager, FlytrapSceneTextView);
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
    // Do NOT stop the portal here — the session persists across the menu and
    // sub-views. It is stopped explicitly via the menu's "Stop Portal" or on
    // app exit.
}
