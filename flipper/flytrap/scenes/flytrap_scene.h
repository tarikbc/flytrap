#pragma once

#include <gui/scene_manager.h>

// Scene id enum
#define ADD_SCENE(prefix, name, id) FlytrapScene##id,
typedef enum {
#include "flytrap_scene_config.h"
    FlytrapSceneNum,
} FlytrapScene;
#undef ADD_SCENE

extern const SceneManagerHandlers flytrap_scene_handlers;

// Handler prototypes
#define ADD_SCENE(prefix, name, id)                                      \
    void prefix##_scene_##name##_on_enter(void*);                        \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "flytrap_scene_config.h"
#undef ADD_SCENE
