#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct FlytrapApp FlytrapApp;

// Create /ext/apps_data/flytrap{,/portals,/logs} if missing.
void flytrap_storage_ensure_dirs(void);

// Load the persisted SSID into app->ssid (defaults to "Free WiFi" if none).
void flytrap_storage_load_config(FlytrapApp* app);

// Persist app->ssid to config.txt.
void flytrap_storage_save_config(FlytrapApp* app);

// Read an HTML file into a freshly malloc'd, NUL-terminated buffer (caller frees).
// Size is capped at FLYTRAP_HTML_MAX. Returns false (and frees) on error/empty.
bool flytrap_storage_read_html(const char* path, uint8_t** out_buf, size_t* out_size);

// Resolve the next logs/capture_<N>.txt into app->log_path for this session.
void flytrap_storage_new_log(FlytrapApp* app);

// Append "<tag>: <text>\n" to the session log (no-op if no log path set).
void flytrap_storage_append_log(FlytrapApp* app, const char* tag, const char* text);
