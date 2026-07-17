#include "../flytrap_i.h"

static void flytrap_clients_list_cb(void* context, uint32_t index) {
    FlytrapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index); // index = position (0=first)
}

static void flytrap_clients_build(FlytrapApp* app) {
    uint32_t sel = submenu_get_selected_item(app->submenu);
    submenu_reset(app->submenu);

    FuriString* s = furi_string_alloc();
    furi_string_printf(s, "Clients (%u)", app->client_count);
    submenu_set_header(app->submenu, furi_string_get_cstr(s));

    for(uint16_t n = 0; n < app->client_count; n++) {
        FlytrapClient* c = &app->clients[n];
        if(c->ip[0]) {
            furi_string_printf(s, "%s  %s", c->ip, c->mac);
        } else {
            furi_string_printf(s, "%s", c->mac); // no IP yet (DHCP pending)
        }
        submenu_add_item(app->submenu, furi_string_get_cstr(s), n, flytrap_clients_list_cb, app);
    }
    if(app->client_count == 0) {
        submenu_add_item(app->submenu, "(no clients connected)", 0xFF, flytrap_clients_list_cb, app);
    }
    furi_string_free(s);

    if(app->client_count > 0) {
        if(sel >= app->client_count) sel = app->client_count - 1;
        submenu_set_selected_item(app->submenu, sel);
    }
    // Remember the revision we built for, so RefreshView only rebuilds on change.
    scene_manager_set_scene_state(app->scene_manager, FlytrapSceneClientsList, app->clients_rev);
}

void flytrap_scene_clients_list_on_enter(void* context) {
    FlytrapApp* app = context;
    submenu_reset(app->submenu);
    flytrap_clients_build(app);
    submenu_set_selected_item(app->submenu, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
}

bool flytrap_scene_clients_list_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == FlytrapEventRefreshView) {
        // Live-update: a client joined/left or got an IP while the list is open.
        uint32_t last = scene_manager_get_scene_state(app->scene_manager, FlytrapSceneClientsList);
        if(app->clients_rev != last) flytrap_clients_build(app);
        return true;
    }
    if(event.event < FLYTRAP_CLIENT_SLOTS && app->client_count > 0) {
        app->selected_client = (uint8_t)event.event;
        scene_manager_next_scene(app->scene_manager, FlytrapSceneClientDetail);
    }
    return true;
}

void flytrap_scene_clients_list_on_exit(void* context) {
    FlytrapApp* app = context;
    submenu_reset(app->submenu);
}
