#include "../flytrap_i.h"

static const char* flytrap_textview_source(FlytrapApp* app) {
    const FuriString* s = NULL;
    const char* empty = "";
    switch(app->textview_mode) {
    case TextViewCaptures:
        s = app->session_captures;
        empty = "(no captures yet)\nSubmit a form on the\ncaptive page, then\nreopen this list.";
        break;
    case TextViewConsole:
        s = app->session_raw;
        empty = "(no ESP output yet)";
        break;
    case TextViewLogFile:
        s = app->logfile_buf;
        empty = "(empty log)";
        break;
    }
    if(!s || furi_string_empty(s)) return empty;
    return furi_string_get_cstr(s);
}

void flytrap_scene_textview_on_enter(void* context) {
    FlytrapApp* app = context;
    // Take a STABLE snapshot into a pre-reserved buffer. The TextBox stores this
    // pointer and formats it lazily on the gui-service thread; the live session
    // buffers keep being mutated on the app thread, so we must never hand the
    // TextBox a pointer into them. The snapshot is written only here (once per
    // open) and never again while this view is up -> no cross-thread mutation.
    furi_string_set_str(app->textview_snapshot, flytrap_textview_source(app));
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->textview_snapshot));
    text_box_set_focus(app->text_box, TextBoxFocusEnd); // newest at bottom
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewTextBox);
}

bool flytrap_scene_textview_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    // Intentionally do NOT re-render on FlytrapEventRefreshView: the snapshot is
    // static so the user can scroll freely, and refreshing would both re-pin the
    // scroll to the bottom and re-alias a live buffer. New captures still fire
    // alerts; reopen the list to see them. (Consume refresh events so they don't
    // propagate as unhandled.)
    if(event.type == SceneManagerEventTypeCustom && event.event == FlytrapEventRefreshView) {
        return true;
    }
    return false;
}

void flytrap_scene_textview_on_exit(void* context) {
    FlytrapApp* app = context;
    text_box_reset(app->text_box);
}
