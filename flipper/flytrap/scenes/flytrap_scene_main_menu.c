#include "../flytrap_i.h"

typedef enum {
    MenuSelectPortal,
    MenuSetSsid,
    MenuStartPortal,
    MenuAbout,
} MainMenuIndex;

static void flytrap_menu_callback(void* context, uint32_t index) {
    FlytrapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void flytrap_pick_portal(FlytrapApp* app) {
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, ".html", NULL);
    opts.base_path = FLYTRAP_PORTALS_DIR;

    FuriString* start = furi_string_alloc_set_str(FLYTRAP_PORTALS_DIR);
    FuriString* result = furi_string_alloc();
    if(!furi_string_empty(app->portal_path)) {
        furi_string_set(result, app->portal_path);
    } else {
        furi_string_set(result, FLYTRAP_PORTALS_DIR);
    }

    if(dialog_file_browser_show(app->dialogs, result, start, &opts)) {
        furi_string_set(app->portal_path, result);
    }
    furi_string_free(start);
    furi_string_free(result);
}

static void flytrap_show_message(FlytrapApp* app, const char* header, const char* text) {
    DialogMessage* m = dialog_message_alloc();
    if(header) dialog_message_set_header(m, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(m, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, m);
    dialog_message_free(m);
}

static void flytrap_menu_build(FlytrapApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Flytrap");
    submenu_add_item(app->submenu, "Select Portal", MenuSelectPortal, flytrap_menu_callback, app);
    submenu_add_item(app->submenu, "Set SSID", MenuSetSsid, flytrap_menu_callback, app);
    submenu_add_item(app->submenu, "Start Portal", MenuStartPortal, flytrap_menu_callback, app);
    submenu_add_item(app->submenu, "About", MenuAbout, flytrap_menu_callback, app);
}

void flytrap_scene_main_menu_on_enter(void* context) {
    FlytrapApp* app = context;
    flytrap_menu_build(app);
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, FlytrapSceneMainMenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
}

bool flytrap_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, FlytrapSceneMainMenu, event.event);

    switch(event.event) {
    case MenuSelectPortal:
        flytrap_pick_portal(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        return true;
    case MenuSetSsid:
        scene_manager_next_scene(app->scene_manager, FlytrapSceneSsidInput);
        return true;
    case MenuStartPortal:
        if(furi_string_empty(app->portal_path)) {
            flytrap_show_message(app, "Flytrap", "Select a portal first.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        } else {
            scene_manager_next_scene(app->scene_manager, FlytrapSceneLive);
        }
        return true;
    case MenuAbout:
        flytrap_show_message(
            app,
            "Flytrap",
            "ESP32 captive portal.\nAuthorized testing only.");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        return true;
    default:
        return false;
    }
}

void flytrap_scene_main_menu_on_exit(void* context) {
    FlytrapApp* app = context;
    submenu_reset(app->submenu);
}
