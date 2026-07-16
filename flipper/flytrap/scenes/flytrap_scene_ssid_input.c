#include "../flytrap_i.h"
#include "../helpers/flytrap_storage.h"

static void flytrap_ssid_input_callback(void* context) {
    FlytrapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlytrapEventSsidDone);
}

void flytrap_scene_ssid_input_on_enter(void* context) {
    FlytrapApp* app = context;

    // Seed the editor with the current SSID.
    strncpy(app->ssid_buf, furi_string_get_cstr(app->ssid), FLYTRAP_SSID_MAX - 1);
    app->ssid_buf[FLYTRAP_SSID_MAX - 1] = '\0';

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Portal SSID (AP name)");
    text_input_set_result_callback(
        app->text_input,
        flytrap_ssid_input_callback,
        app,
        app->ssid_buf,
        FLYTRAP_SSID_MAX,
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewTextInput);
}

bool flytrap_scene_ssid_input_on_event(void* context, SceneManagerEvent event) {
    FlytrapApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == FlytrapEventSsidDone) {
        if(strlen(app->ssid_buf) > 0) {
            furi_string_set(app->ssid, app->ssid_buf);
            flytrap_storage_save_config(app);
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void flytrap_scene_ssid_input_on_exit(void* context) {
    UNUSED(context);
}
