#include "../flytrap_i.h"
#include "../helpers/flytrap_format.h"

static uint16_t flytrap_captures_shown(FlytrapApp* app) {
    return app->cap_count < FLYTRAP_CAP_SLOTS ? app->cap_count : FLYTRAP_CAP_SLOTS;
}

static void flytrap_detail_button_cb(GuiButtonType result, InputType type, void* context) {
    FlytrapApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventCapturePrev);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventCaptureNext);
    }
}

static void flytrap_detail_render(FlytrapApp* app) {
    uint16_t shown = flytrap_captures_shown(app);
    if(shown == 0) return;
    if(app->selected_capture >= shown) app->selected_capture = shown - 1;

    uint8_t pos = app->selected_capture; // 0 = newest
    uint8_t idx = (app->cap_head + FLYTRAP_CAP_SLOTS - 1 - pos) % FLYTRAP_CAP_SLOTS;
    FlytrapCapture* c = &app->captures[idx];

    widget_reset(app->widget);

    FuriString* text = furi_string_alloc();
    furi_string_cat_printf(text, "\ec%u/%u   %s\n", pos + 1, shown, c->when); // centered header
    FuriString* fields = furi_string_alloc();
    flytrap_kv_pretty(c->raw, fields);
    furi_string_cat(text, fields);
    furi_string_free(fields);
    // Scroll region above the button bar.
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, furi_string_get_cstr(text));
    furi_string_free(text);

    // Page through captures with Left/Right (Up/Down scroll the fields).
    if(pos + 1 < shown) {
        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "Prev", flytrap_detail_button_cb, app);
    }
    if(pos > 0) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Next", flytrap_detail_button_cb, app);
    }
}

void flytrap_scene_capture_detail_on_enter(void* context) {
    FlytrapApp* app = context;
    flytrap_detail_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
}

bool flytrap_scene_capture_detail_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case FlytrapEventCapturePrev: // older
        if(app->selected_capture + 1 < flytrap_captures_shown(app)) {
            app->selected_capture++;
            flytrap_detail_render(app);
        }
        return true;
    case FlytrapEventCaptureNext: // newer
        if(app->selected_capture > 0) {
            app->selected_capture--;
            flytrap_detail_render(app);
        }
        return true;
    case FlytrapEventRefreshView:
        return true;
    default:
        return false;
    }
}

void flytrap_scene_capture_detail_on_exit(void* context) {
    FlytrapApp* app = context;
    widget_reset(app->widget);
}
