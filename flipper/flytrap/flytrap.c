#include "flytrap_i.h"
#include "helpers/flytrap_storage.h"
#include "helpers/flytrap_session.h"

static bool flytrap_custom_event_callback(void* context, uint32_t event) {
    FlytrapApp* app = context;
    if(event == FlytrapEventRxData) {
        if(app->session_active) {
            flytrap_session_rx(app);
            // Let whichever view is on top (dashboard / captures / console) redraw.
            return scene_manager_handle_custom_event(app->scene_manager, FlytrapEventRefreshView);
        }
        // Idle: discard so the RX stream can't slowly fill.
        uint8_t junk[64];
        while(flytrap_uart_rx(app->uart, junk, sizeof(junk)) > 0) {
        }
        return true;
    }
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flytrap_back_event_callback(void* context) {
    FlytrapApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// Runs on the UART worker thread: just wake the GUI to drain/parse RX bytes.
// The context is fixed for the app's lifetime, so this never races a toggle.
static void flytrap_uart_notify(void* ctx) {
    FlytrapApp* app = ctx;
    // Once we're tearing down, the dispatcher's event loop has stopped draining;
    // posting here would block the worker forever on a full queue (deadlocking
    // teardown). Bail out so the worker parks in its flag-wait and joins cleanly.
    if(app->closing) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventRxData);
}

static FlytrapApp* flytrap_app_alloc(void) {
    FlytrapApp* app = malloc(sizeof(FlytrapApp));
    memset(app, 0, sizeof(FlytrapApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&flytrap_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, flytrap_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, flytrap_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlytrapViewSubmenu, submenu_get_view(app->submenu));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FlytrapViewTextInput, text_input_get_view(app->text_input));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FlytrapViewWidget, widget_get_view(app->widget));
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlytrapViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->ssid = furi_string_alloc();
    app->portal_path = furi_string_alloc();
    app->line_acc = furi_string_alloc();
    app->status = furi_string_alloc_set_str("idle");
    app->log_path = furi_string_alloc();
    app->legacy_user = furi_string_alloc();
    app->session_raw = furi_string_alloc();
    app->logfile_buf = furi_string_alloc();

    app->sound_on = true; // defaults; overridden by config if present
    app->vibro_on = true;
    flytrap_storage_ensure_dirs();
    flytrap_storage_load_config(app);

    app->uart = flytrap_uart_init(115200, flytrap_uart_notify, app);

    scene_manager_next_scene(app->scene_manager, FlytrapSceneMainMenu);
    return app;
}

static void flytrap_app_free(FlytrapApp* app) {
    // Tear down in an order that can't deadlock: stop the RX worker BEFORE the
    // ViewDispatcher is freed (so it can't block posting events to a dead queue),
    // and guard the notify against the tiny pre-stop window.
    app->closing = true;
    flytrap_uart_stop_rx(app->uart);

    // Now safe to reset the ESP (AP down); its reply is ignored since RX is off.
    flytrap_uart_tx(app->uart, (const uint8_t*)"reset\n", 6);
    furi_delay_ms(20);

    view_dispatcher_remove_view(app->view_dispatcher, FlytrapViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, FlytrapViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, FlytrapViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, FlytrapViewVarItemList);

    submenu_free(app->submenu);
    text_input_free(app->text_input);
    widget_free(app->widget);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    flytrap_uart_deinit(app->uart);

    furi_string_free(app->ssid);
    furi_string_free(app->portal_path);
    furi_string_free(app->line_acc);
    furi_string_free(app->status);
    furi_string_free(app->log_path);
    furi_string_free(app->legacy_user);
    furi_string_free(app->session_raw);
    furi_string_free(app->logfile_buf);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t flytrap_app(void* p) {
    UNUSED(p);
    FlytrapApp* app = flytrap_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    flytrap_app_free(app);
    return 0;
}
