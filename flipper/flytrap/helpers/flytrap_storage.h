#pragma once

#include <furi.h>

typedef struct FlytrapApp FlytrapApp;

void flytrap_storage_ensure_dirs(void);

// Config (FlipperFormat): SSID + last-selected PortalPath.
void flytrap_storage_load_config(FlytrapApp* app);
void flytrap_storage_save_config(FlytrapApp* app);

// Read a text file (HTML portal, saved log, ...) into `out`, capped at `cap`
// bytes. Returns false on error/empty.
bool flytrap_storage_read_file(const char* path, FuriString* out, size_t cap);

// Resolve the next logs/capture_<N>.txt into app->log_path for this session.
void flytrap_storage_new_log(FlytrapApp* app);

// Append "<tag>: <text>\n" (timestamped) to the session log.
void flytrap_storage_append_log(FlytrapApp* app, const char* tag, const char* text);

// Fill `out` with a "[HH:MM:SS] " RTC prefix.
void flytrap_timestamp(FuriString* out);
