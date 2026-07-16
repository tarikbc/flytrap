#include "flytrap_scene.h"

// on_enter handlers
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const flytrap_scene_on_enter_handlers[])(void*) = {
#include "flytrap_scene_config.h"
};
#undef ADD_SCENE

// on_event handlers
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const flytrap_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "flytrap_scene_config.h"
};
#undef ADD_SCENE

// on_exit handlers
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const flytrap_scene_on_exit_handlers[])(void* context) = {
#include "flytrap_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers flytrap_scene_handlers = {
    .on_enter_handlers = flytrap_scene_on_enter_handlers,
    .on_event_handlers = flytrap_scene_on_event_handlers,
    .on_exit_handlers = flytrap_scene_on_exit_handlers,
    .scene_num = FlytrapSceneNum,
};
