#include "../flytrap_i.h"

static void flytrap_client_detail_button_cb(GuiButtonType result, InputType type, void* context) {
    FlytrapApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventClientPrev);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventClientNext);
    }
}

static void flytrap_client_detail_render(FlytrapApp* app) {
    widget_reset(app->widget);

    if(app->client_count == 0) {
        widget_add_string_element(
            app->widget, 64, 28, AlignCenter, AlignCenter, FontSecondary, "Client disconnected");
        return;
    }
    if(app->selected_client >= app->client_count) app->selected_client = app->client_count - 1;

    uint8_t pos = app->selected_client;
    FlytrapClient* c = &app->clients[pos];

    FuriString* tmp = furi_string_alloc();

    furi_string_printf(tmp, "Client %u/%u", pos + 1, app->client_count);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(tmp));
    widget_add_line_element(app->widget, 0, 14, 127, 14);

    furi_string_printf(tmp, "MAC:  %s", c->mac);
    widget_add_string_element(
        app->widget, 0, 18, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    furi_string_printf(tmp, "IP:   %s", c->ip[0] ? c->ip : "(pending)");
    widget_add_string_element(
        app->widget, 0, 30, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    furi_string_printf(tmp, "Joined: %s", c->when);
    widget_add_string_element(
        app->widget, 0, 42, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(tmp));

    furi_string_free(tmp);

    // Page through clients with Left/Right.
    if(pos + 1 < app->client_count) {
        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "Prev", flytrap_client_detail_button_cb, app);
    }
    if(pos > 0) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Next", flytrap_client_detail_button_cb, app);
    }
}

void flytrap_scene_client_detail_on_enter(void* context) {
    FlytrapApp* app = context;
    flytrap_client_detail_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewWidget);
}

bool flytrap_scene_client_detail_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case FlytrapEventClientPrev:
        if(app->selected_client + 1 < app->client_count) {
            app->selected_client++;
            flytrap_client_detail_render(app);
        }
        return true;
    case FlytrapEventClientNext:
        if(app->selected_client > 0) {
            app->selected_client--;
            flytrap_client_detail_render(app);
        }
        return true;
    case FlytrapEventRefreshView:
        // A client left/joined or got an IP — re-render so the details stay current.
        flytrap_client_detail_render(app);
        return true;
    default:
        return false;
    }
}

void flytrap_scene_client_detail_on_exit(void* context) {
    FlytrapApp* app = context;
    widget_reset(app->widget);
}
