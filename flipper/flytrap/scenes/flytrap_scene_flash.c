#include "../flytrap_i.h"
#include "../helpers/flytrap_flasher.h"

// Runs on the flashing worker thread — only touch volatile progress fields and
// signal the GUI (never block or render here).
static void
    flytrap_flash_progress_cb(void* ctx, uint8_t idx, uint8_t cnt, uint8_t pct, const char* stage) {
    FlytrapApp* app = ctx;
    app->flash_img = idx;
    app->flash_cnt = cnt;
    app->flash_pct = pct;
    strncpy(app->flash_stage, stage, sizeof(app->flash_stage) - 1);
    app->flash_stage[sizeof(app->flash_stage) - 1] = '\0';
    if(!app->closing)
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventFlashProgress);
}

// Called (on the worker thread) the moment the board answers in download mode.
static void flytrap_flash_on_connected(void* ctx) {
    FlytrapApp* app = ctx;
    app->flashing = true; // lock out Back now that writing is imminent
    if(!app->closing)
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventFlashStart);
}

static int32_t flytrap_flash_worker(void* ctx) {
    FlytrapApp* app = ctx;
    char err[80] = "";
    bool ok = flytrap_flasher_run(
        app->uart,
        furi_string_get_cstr(app->flash_manifest),
        flytrap_flash_progress_cb,
        app,
        &app->flash_cancel,
        flytrap_flash_on_connected,
        err,
        sizeof(err));
    app->flash_ok = ok;
    strncpy(
        app->flash_msg,
        ok ? "Board rebooting with\nthe new firmware." : err,
        sizeof(app->flash_msg) - 1);
    app->flash_msg[sizeof(app->flash_msg) - 1] = '\0';
    app->flashing = false;
    // A cancel (Back while waiting) isn't a result to show — the scene is gone.
    if(!app->closing && !app->flash_cancel)
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventFlashDone);
    return 0;
}

static void flytrap_flash_render(FlytrapApp* app) {
    widget_reset(app->widget);
    FuriString* t = furi_string_alloc();

    if(app->flash_phase == 0) {
        widget_add_string_element(
            app->widget, 64, 3, AlignCenter, AlignTop, FontPrimary, "Download mode");
        widget_add_line_element(app->widget, 0, 16, 127, 16);
        widget_add_string_multiline_element(
            app->widget,
            64,
            20,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "On the board: hold BOOT,\ntap RESET, release BOOT.\nWaiting for it...");
    } else if(app->flash_phase == 1) {
        widget_add_string_element(
            app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "Flashing...");
        widget_add_line_element(app->widget, 0, 20, 127, 20);
        if(app->flash_cnt)
            furi_string_printf(
                t, "Image %u/%u: %s", app->flash_img + 1, app->flash_cnt, app->flash_stage);
        else
            furi_string_set(t, app->flash_stage);
        widget_add_string_element(
            app->widget, 64, 26, AlignCenter, AlignTop, FontSecondary, furi_string_get_cstr(t));
        furi_string_printf(t, "%u%%", app->flash_pct);
        widget_add_string_element(
            app->widget, 64, 39, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(t));
        widget_add_string_element(
            app->widget, 64, 54, AlignCenter, AlignTop, FontSecondary, "Do not disconnect");
    } else {
        widget_add_string_element(
            app->widget,
            64,
            6,
            AlignCenter,
            AlignTop,
            FontPrimary,
            app->flash_ok ? "Flashed!" : "Flash failed");
        widget_add_line_element(app->widget, 0, 20, 127, 20);
        widget_add_string_multiline_element(
            app->widget, 64, 26, AlignCenter, AlignTop, FontSecondary, app->flash_msg);
    }
    furi_string_free(t);
}

void flytrap_scene_flash_on_enter(void* context) {
    FlytrapApp* app = context;
    app->flash_phase = 0;
    app->flash_pct = 0;
    app->flash_img = 0;
    app->flash_cnt = 0;
    app->flash_stage[0] = '\0';
    app->flashing = false;
    app->flash_cancel = false;
    flytrap_flash_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
    // Auto-detect: the worker polls for download mode and flashes when it appears.
    // Generous stack: the stub-loader connect + flash call chain is deep, and a
    // worker-thread overflow surfaces as an "out of memory" crash.
    app->flash_thread = furi_thread_alloc_ex("FlytrapFlash", 16384, flytrap_flash_worker, app);
    furi_thread_start(app->flash_thread);
}

bool flytrap_scene_flash_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case FlytrapEventFlashStart: // board detected — switch to the flashing screen
        app->flash_phase = 1;
        flytrap_flash_render(app);
        return true;
    case FlytrapEventFlashProgress:
        if(app->flash_phase == 1) flytrap_flash_render(app);
        return true;
    case FlytrapEventFlashDone:
        app->flash_phase = 2;
        if(app->flash_thread) {
            furi_thread_join(app->flash_thread);
            furi_thread_free(app->flash_thread);
            app->flash_thread = NULL;
        }
        flytrap_flash_render(app);
        return true;
    case FlytrapEventRefreshView:
        return true; // ignore any stray portal RX refresh here
    default:
        return false;
    }
}

void flytrap_scene_flash_on_exit(void* context) {
    FlytrapApp* app = context;
    // Stop the poll (if still waiting) and reap the worker. Back is blocked while
    // flashing, so this only runs before connect or after done.
    app->flash_cancel = true;
    if(app->flash_thread) {
        furi_thread_join(app->flash_thread);
        furi_thread_free(app->flash_thread);
        app->flash_thread = NULL;
    }
    app->flashing = false;
    widget_reset(app->widget);
}
