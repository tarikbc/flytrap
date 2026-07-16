#include "../flytrap_i.h"
#include "../helpers/flytrap_storage.h"

static const char* const on_off[] = {"OFF", "ON"};

static void flytrap_settings_vibro_cb(VariableItem* item) {
    FlytrapApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->vibro_on = (i != 0);
    variable_item_set_current_value_text(item, on_off[i]);
    flytrap_storage_save_config(app);
}

static void flytrap_settings_sound_cb(VariableItem* item) {
    FlytrapApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->sound_on = (i != 0);
    variable_item_set_current_value_text(item, on_off[i]);
    flytrap_storage_save_config(app);
}

void flytrap_scene_settings_on_enter(void* context) {
    FlytrapApp* app = context;
    variable_item_list_reset(app->var_item_list);

    VariableItem* item;
    item = variable_item_list_add(
        app->var_item_list, "Vibration", 2, flytrap_settings_vibro_cb, app);
    variable_item_set_current_value_index(item, app->vibro_on ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->vibro_on ? 1 : 0]);

    item =
        variable_item_list_add(app->var_item_list, "Sound", 2, flytrap_settings_sound_cb, app);
    variable_item_set_current_value_index(item, app->sound_on ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->sound_on ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlytrapViewVarItemList);
}

bool flytrap_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flytrap_scene_settings_on_exit(void* context) {
    FlytrapApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
