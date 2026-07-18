#include "../flytrap_i.h"
#include "../helpers/flytrap_storage.h"
#include "../helpers/flytrap_session.h"

typedef enum {
    MenuStartOrDashboard,
    MenuStop,
    MenuConsole,
    MenuSelectPortal,
    MenuSetSsid,
    MenuFlashFirmware,
    MenuViewLogs,
    MenuSettings,
    MenuAbout,
} MainMenuIndex;

static const char* flytrap_basename(const char* path) {
    const char* slash = strrchr(path, '/');
    return (slash && *(slash + 1)) ? slash + 1 : path;
}

static void flytrap_menu_callback(void* context, uint32_t index) {
    FlytrapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void flytrap_show_message(FlytrapApp* app, const char* header, const char* text) {
    DialogMessage* m = dialog_message_alloc();
    if(header) dialog_message_set_header(m, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(m, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, m);
    dialog_message_free(m);
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
        flytrap_storage_save_config(app); // persist selection
    }
    furi_string_free(start);
    furi_string_free(result);
}

// Pick a firmware manifest (flash.txt) from the firmware folder. Returns true if
// one was selected (stored in app->flash_manifest).
static bool flytrap_pick_firmware(FlytrapApp* app) {
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, ".txt", NULL);
    opts.base_path = FLYTRAP_FIRMWARE_DIR;

    FuriString* start = furi_string_alloc_set_str(FLYTRAP_FIRMWARE_DIR);
    FuriString* result = furi_string_alloc_set_str(FLYTRAP_FIRMWARE_DIR);
    bool picked = dialog_file_browser_show(app->dialogs, result, start, &opts);
    if(picked) furi_string_set(app->flash_manifest, result);
    furi_string_free(start);
    furi_string_free(result);
    return picked;
}

// Returns true if it navigated to the log viewer (caller should NOT re-switch
// the view), false if the user cancelled.
static bool flytrap_view_logs(FlytrapApp* app) {
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, ".txt", NULL);
    opts.base_path = FLYTRAP_LOGS_DIR;

    FuriString* start = furi_string_alloc_set_str(FLYTRAP_LOGS_DIR);
    FuriString* result = furi_string_alloc_set_str(FLYTRAP_LOGS_DIR);
    bool picked = dialog_file_browser_show(app->dialogs, result, start, &opts);
    if(picked) {
        flytrap_storage_read_file(
            furi_string_get_cstr(result), app->logfile_buf, FLYTRAP_SESSION_BUF_MAX * 2);
        app->textview_mode = TextViewLogFile;
    }
    furi_string_free(start);
    furi_string_free(result);
    if(picked) scene_manager_next_scene(app->scene_manager, FlytrapSceneTextView);
    return picked;
}

static void flytrap_menu_build(FlytrapApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->session_active ? "Flytrap  [ON]" : "Flytrap");

    if(app->session_active) {
        submenu_add_item(
            app->submenu, "Portal Dashboard", MenuStartOrDashboard, flytrap_menu_callback, app);
        submenu_add_item(app->submenu, "Stop Portal", MenuStop, flytrap_menu_callback, app);
        submenu_add_item(app->submenu, "Console", MenuConsole, flytrap_menu_callback, app);
    } else {
        submenu_add_item(
            app->submenu, "Start Portal", MenuStartOrDashboard, flytrap_menu_callback, app);
    }

    FuriString* label = furi_string_alloc();
    furi_string_printf(
        label,
        "Portal: %s",
        furi_string_empty(app->portal_path) ? "(none)" :
                                              flytrap_basename(furi_string_get_cstr(app->portal_path)));
    submenu_add_item(
        app->submenu, furi_string_get_cstr(label), MenuSelectPortal, flytrap_menu_callback, app);

    furi_string_printf(label, "SSID: %s", furi_string_get_cstr(app->ssid));
    submenu_add_item(
        app->submenu, furi_string_get_cstr(label), MenuSetSsid, flytrap_menu_callback, app);
    furi_string_free(label);

    submenu_add_item(
        app->submenu, "Flash Firmware", MenuFlashFirmware, flytrap_menu_callback, app);
    submenu_add_item(app->submenu, "View Logs", MenuViewLogs, flytrap_menu_callback, app);
    submenu_add_item(app->submenu, "Settings", MenuSettings, flytrap_menu_callback, app);
    submenu_add_item(app->submenu, "About", MenuAbout, flytrap_menu_callback, app);

    app->menu_shows_active = app->session_active; // remember what this build reflects
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
    if(event.event == FlytrapEventRefreshView) {
        // Live RX is otherwise irrelevant at the menu, but the session can end
        // under us (board unplugged -> link lost) — rebuild so Start/Stop match.
        if(app->session_active != app->menu_shows_active) {
            flytrap_menu_build(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        }
        return true;
    }

    scene_manager_set_scene_state(app->scene_manager, FlytrapSceneMainMenu, event.event);

    switch(event.event) {
    case MenuStartOrDashboard:
        if(app->session_active) {
            scene_manager_next_scene(app->scene_manager, FlytrapSceneLive);
        } else if(furi_string_empty(app->portal_path)) {
            flytrap_show_message(app, "Flytrap", "Select a portal first.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        } else {
            // The Live scene walks the start flow (detect board → send portal),
            // painting a status screen before each blocking step.
            scene_manager_next_scene(app->scene_manager, FlytrapSceneLive);
        }
        return true;
    case MenuStop:
        flytrap_session_stop(app);
        flytrap_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        return true;
    case MenuConsole:
        app->textview_mode = TextViewConsole;
        scene_manager_next_scene(app->scene_manager, FlytrapSceneTextView);
        return true;
    case MenuSelectPortal:
        flytrap_pick_portal(app);
        flytrap_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        return true;
    case MenuSetSsid:
        scene_manager_next_scene(app->scene_manager, FlytrapSceneSsidInput);
        return true;
    case MenuFlashFirmware:
        if(app->session_active) {
            // The ESP must be in its download bootloader, not running the portal.
            flytrap_show_message(app, "Flash Firmware", "Stop the portal first.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        } else if(flytrap_pick_firmware(app)) {
            scene_manager_next_scene(app->scene_manager, FlytrapSceneFlash);
        } else {
            view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        }
        return true;
    case MenuSettings:
        scene_manager_next_scene(app->scene_manager, FlytrapSceneSettings);
        return true;
    case MenuViewLogs:
        if(!flytrap_view_logs(app)) {
            view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewSubmenu);
        }
        return true;
    case MenuAbout:
        flytrap_show_message(app, "Flytrap", "ESP32 captive portal.\nAuthorized testing only.");
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
