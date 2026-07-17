#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct FlytrapApp FlytrapApp;

// Start a portal session: reset counters/buffers, open a new log, and send the
// selected portal HTML (with {{SSID}} templated). Sets session_active.
void flytrap_session_start(FlytrapApp* app);

// Stop the portal (sends "stop" — tears down the AP without rebooting the ESP).
void flytrap_session_stop(FlytrapApp* app);

// Drain the UART and parse all pending lines. Call on the GUI thread only.
// Also re-runs the handshake if the ESP reported "boot" mid-session.
void flytrap_session_rx(FlytrapApp* app);

// Is the ESP board attached and talking? True if we've heard traffic (its ~2s
// PING beacon) very recently, else waits up to wait_ms for the next beacon.
// Call on the GUI thread only (it may briefly block).
bool flytrap_board_present(FlytrapApp* app, uint32_t wait_ms);
