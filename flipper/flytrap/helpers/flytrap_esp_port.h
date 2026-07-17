#pragma once

#include <furi_hal_serial.h>
#include <stdint.h>

// Flipper "port" for espressif/esp-serial-flasher: it calls the loader_port_*
// functions (implemented in the .c) to talk to the ESP ROM bootloader over our
// GPIO USART. Attach it to an already-acquired serial handle before calling
// esp_loader_connect(), and detach when finished.
//
// The board's reset/boot pins aren't wired to controllable Flipper GPIO, so the
// user puts the ESP in download mode by hand (hold BOOT, tap RESET) — the port's
// enter_bootloader/reset_target are therefore no-ops.
void flytrap_esp_port_start(FuriHalSerialHandle* handle, uint32_t baud);
void flytrap_esp_port_stop(void);
