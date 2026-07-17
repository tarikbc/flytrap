#pragma once

#include <furi.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "flytrap_uart.h"
#include "flytrap_custom_event.h"
#include "scenes/flytrap_scene.h"

#define FLYTRAP_SSID_MAX (33) // 32 chars + NUL
#define FLYTRAP_HTML_MAX (48000)
#define FLYTRAP_CAP_SLOTS (32)
#define FLYTRAP_CLIENT_SLOTS (16)
#define FLYTRAP_CAP_KV_SIZE (160)
#define FLYTRAP_LINE_MAX (512)
#define FLYTRAP_LINK_TIMEOUT_MS (5000) // no PING/data for this long -> board unplugged
#define FLYTRAP_SESSION_BUF_MAX (4096) // scrollable captures/raw buffers

#define FLYTRAP_DATA_DIR EXT_PATH("apps_data/flytrap")
#define FLYTRAP_PORTALS_DIR FLYTRAP_DATA_DIR "/portals"
#define FLYTRAP_FIRMWARE_DIR FLYTRAP_DATA_DIR "/firmware"
#define FLYTRAP_CONFIG_PATH FLYTRAP_DATA_DIR "/config.txt"
#define FLYTRAP_LOGS_DIR FLYTRAP_DATA_DIR "/logs"
#define FLYTRAP_SSID_TOKEN "{{SSID}}" // replaced with the configured SSID in portals

typedef enum {
    FlytrapViewSubmenu,
    FlytrapViewTextInput,
    FlytrapViewWidget,
    FlytrapViewVarItemList,
} FlytrapView;

// Which content the shared textview scene shows (scrolling raw text).
typedef enum {
    TextViewConsole,
    TextViewLogFile,
} FlytrapTextViewMode;

typedef struct {
    char when[12]; // "HH:MM:SS"
    char ip[20]; // client IP, if the firmware reported one
    char raw[FLYTRAP_CAP_KV_SIZE]; // urlencoded k=v&... (decoded on demand)
} FlytrapCapture;

// A currently-connected station. Left clients are removed, so the list and
// count always reflect who is online right now.
typedef struct {
    char mac[18]; // "AA:BB:CC:DD:EE:FF"
    char ip[20]; // assigned IP, or "" until DHCP reports one
    char when[12]; // "HH:MM:SS" joined
} FlytrapClient;

typedef struct FlytrapApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    VariableItemList* var_item_list;

    FlytrapUart* uart;

    // Config / selection (persisted via FlipperFormat)
    FuriString* ssid;
    FuriString* portal_path;
    char ssid_buf[FLYTRAP_SSID_MAX];
    bool sound_on; // capture alert: beep + LED
    bool vibro_on; // capture alert: haptic

    // Live session state (all touched only on the GUI thread)
    FuriString* line_acc; // RX line assembler
    FuriString* status; // last status token
    FuriString* log_path; // this session's capture log path
    FuriString* legacy_user; // legacy "u:" held until "p:"
    FuriString* session_raw; // scrollable raw ESP output (this session, console view)
    FuriString* logfile_buf; // a saved log loaded for viewing
    FlytrapCapture captures[FLYTRAP_CAP_SLOTS];
    uint8_t cap_head;
    uint16_t cap_count;

    // Live clients: clients[0..client_count-1] are the stations connected now.
    FlytrapClient clients[FLYTRAP_CLIENT_SLOTS];
    uint16_t client_count;
    uint32_t clients_rev; // bumped on any client change, so views refresh only when needed
    uint8_t selected_client; // index into clients[] for the detail view

    FlytrapTextViewMode textview_mode;

    // Flash Firmware (ESP-serial-flasher). The worker runs off the GUI thread and
    // posts progress/done events; `flashing` blocks Back while it can't be aborted.
    FuriThread* flash_thread;
    FuriString* flash_manifest; // selected flash.txt path
    volatile bool flashing;
    volatile uint8_t flash_img, flash_cnt, flash_pct;
    char flash_stage[40];
    char flash_msg[80];
    bool flash_ok;
    uint8_t flash_phase; // 0=prompt, 1=flashing, 2=done

    // Board liveness: the ESP beacons "PING" ~every 2s; if we hear nothing for
    // FLYTRAP_LINK_TIMEOUT_MS the board is likely unplugged and we flag the link.
    uint32_t last_rx_tick;
    bool link_lost;
    bool awaiting_board; // on the "No board detected" screen, watching for the beacon

    // Handshake / lifecycle flags
    bool portal_running;
    bool menu_shows_active; // session_active the main menu was last built for
    bool pending_setap;
    bool session_active; // portal owned (persists across menu/sub-views)
    bool need_restart; // ESP reported "boot" mid-session -> resend
    volatile bool closing; // app tearing down: worker must stop posting events

    uint8_t selected_capture; // index into captures[] for the detail view
} FlytrapApp;
