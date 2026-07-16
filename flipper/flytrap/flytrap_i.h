#pragma once

#include <furi.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "flytrap_uart.h"
#include "flytrap_custom_event.h"
#include "scenes/flytrap_scene.h"

#define FLYTRAP_SSID_MAX (33) // 32 chars + NUL
#define FLYTRAP_HTML_MAX (24000)
#define FLYTRAP_CAP_SLOTS (8)
#define FLYTRAP_CAP_KV_SIZE (128)
#define FLYTRAP_LINE_MAX (512)
#define FLYTRAP_SESSION_BUF_MAX (4096) // scrollable captures/raw buffers

#define FLYTRAP_DATA_DIR EXT_PATH("apps_data/flytrap")
#define FLYTRAP_PORTALS_DIR FLYTRAP_DATA_DIR "/portals"
#define FLYTRAP_CONFIG_PATH FLYTRAP_DATA_DIR "/config.txt"
#define FLYTRAP_LOGS_DIR FLYTRAP_DATA_DIR "/logs"
#define FLYTRAP_SSID_TOKEN "{{SSID}}" // replaced with the configured SSID in portals

typedef enum {
    FlytrapViewSubmenu,
    FlytrapViewTextInput,
    FlytrapViewWidget,
    FlytrapViewTextBox,
} FlytrapView;

// Which content the shared textview scene shows.
typedef enum {
    TextViewCaptures,
    TextViewConsole,
    TextViewLogFile,
} FlytrapTextViewMode;

typedef struct {
    char kv[FLYTRAP_CAP_KV_SIZE]; // urlencoded captured fields
} FlytrapCapture;

typedef struct FlytrapApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    TextBox* text_box;

    FlytrapUart* uart;

    // Config / selection (persisted via FlipperFormat)
    FuriString* ssid;
    FuriString* portal_path;
    char ssid_buf[FLYTRAP_SSID_MAX];

    // Live session state (all touched only on the GUI thread)
    FuriString* line_acc; // RX line assembler
    FuriString* status; // last status token
    FuriString* log_path; // this session's capture log path
    FuriString* legacy_user; // legacy "u:" held until "p:"
    FuriString* session_captures; // scrollable capture history (this session)
    FuriString* session_raw; // scrollable raw ESP output (this session)
    FuriString* logfile_buf; // a saved log loaded for viewing
    FuriString* textview_snapshot; // stable copy handed to the TextBox (see textview scene)
    FlytrapCapture captures[FLYTRAP_CAP_SLOTS];
    uint8_t cap_head;
    uint16_t cap_count;
    uint16_t client_count;

    FlytrapTextViewMode textview_mode;

    // Handshake / lifecycle flags
    bool portal_running;
    bool pending_setap;
    bool session_active; // portal owned (persists across menu/sub-views)
    bool need_restart; // ESP reported "boot" mid-session -> resend
} FlytrapApp;
