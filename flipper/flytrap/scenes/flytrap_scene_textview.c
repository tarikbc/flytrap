#include "../flytrap_i.h"

// Console / saved-log viewer: a scrolling text widget. widget_add_text_scroll_element
// COPIES its text, so passing a live buffer's cstr is safe (snapshot at add time),
// and we don't re-render on RX (static, so scrolling isn't yanked to the bottom).
static void flytrap_textview_render(FlytrapApp* app) {
    widget_reset(app->widget);
    const char* src;
    if(app->textview_mode == TextViewConsole) {
        src = furi_string_empty(app->session_raw) ? "(no ESP output yet)" :
                                                    furi_string_get_cstr(app->session_raw);
    } else {
        src = furi_string_empty(app->logfile_buf) ? "(empty log)" :
                                                    furi_string_get_cstr(app->logfile_buf);
    }
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, src);
}

void flytrap_scene_textview_on_enter(void* context) {
    FlytrapApp* app = context;
    flytrap_textview_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
}

bool flytrap_scene_textview_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    if(event.type == SceneManagerEventTypeCustom && event.event == FlytrapEventRefreshView) {
        return true; // static snapshot; ignore live RX
    }
    return false;
}

void flytrap_scene_textview_on_exit(void* context) {
    FlytrapApp* app = context;
    widget_reset(app->widget);
}
