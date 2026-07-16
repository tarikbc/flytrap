#include "../flytrap_i.h"

static void flytrap_captures_list_cb(void* context, uint32_t index) {
    FlytrapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index); // index = position (0=newest)
}

static void flytrap_captures_build(FlytrapApp* app) {
    uint32_t sel = submenu_get_selected_item(app->submenu);
    submenu_reset(app->submenu);

    FuriString* s = furi_string_alloc();
    furi_string_printf(s, "Captures (%u)", app->cap_count);
    submenu_set_header(app->submenu, furi_string_get_cstr(s));

    uint16_t shown = app->cap_count < FLYTRAP_CAP_SLOTS ? app->cap_count : FLYTRAP_CAP_SLOTS;
    for(uint16_t n = 0; n < shown; n++) {
        uint8_t idx = (app->cap_head + FLYTRAP_CAP_SLOTS - 1 - n) % FLYTRAP_CAP_SLOTS;
        FlytrapCapture* c = &app->captures[idx];
        if(c->ip[0]) {
            furi_string_printf(s, "%s  %s", c->when, c->ip);
        } else {
            furi_string_printf(s, "%s", c->when);
        }
        submenu_add_item(app->submenu, furi_string_get_cstr(s), n, flytrap_captures_list_cb, app);
    }
    if(shown == 0) {
        submenu_add_item(app->submenu, "(no captures yet)", 0xFF, flytrap_captures_list_cb, app);
    }
    furi_string_free(s);

    if(shown > 0) {
        if(sel >= shown) sel = shown - 1;
        submenu_set_selected_item(app->submenu, sel);
    }
    // Remember the count we built for, so RefreshView only rebuilds on change.
    scene_manager_set_scene_state(app->scene_manager, FlytrapSceneCapturesList, app->cap_count);
}

void flytrap_scene_captures_list_on_enter(void* context) {
    FlytrapApp* app = context;
    submenu_reset(app->submenu); // start cursor at top (newest)
    flytrap_captures_build(app);
    submenu_set_selected_item(app->submenu, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
}

bool flytrap_scene_captures_list_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == FlytrapEventRefreshView) {
        // Live-update: a new capture arrived while the list is open.
        uint32_t last = scene_manager_get_scene_state(app->scene_manager, FlytrapSceneCapturesList);
        if(app->cap_count != last) flytrap_captures_build(app);
        return true;
    }
    if(event.event < FLYTRAP_CAP_SLOTS) {
        app->selected_capture = (uint8_t)event.event;
        scene_manager_next_scene(app->scene_manager, FlytrapSceneCaptureDetail);
    }
    return true;
}

void flytrap_scene_captures_list_on_exit(void* context) {
    FlytrapApp* app = context;
    submenu_reset(app->submenu);
}
