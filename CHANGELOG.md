# Changelog

All notable changes to Flytrap are documented here.

## [0.1.0] — unreleased

First working version — a from-scratch captive-portal suite for the Flipper Zero
and the official ESP32-S2 WiFi dev board.

### Flipper app (`flipper/flytrap`)
- Lean 3-scene / 3-view architecture (main menu, SSID input, live captures) that
  stays well under the RAM ceiling that OOMs heavier tools on this board.
- On-device **portal picker** via the file browser (`/ext/apps_data/flytrap/portals/`).
- **SSID entry** from the UI, persisted to `config.txt`.
- **Live captured-credentials** view (client + credential counters, latest capture)
  with per-session logging to `logs/capture_<N>.txt`.
- Decoupled `furi_hal_serial` UART layer with the required `expansion_disable`
  handshake before acquiring the GPIO USART.
- Builds with `ufbt` (Momentum SDK, Target 7 / API 87.1).

### ESP32-S2 firmware (`esp32/flytrap-fw`)
- Flytrap wire protocol v2: **length-prefixed** `sethtml <N>` (no `Serial.readString`
  payload splitting), bounded copies into the AP-name / HTML buffers, explicit
  `start`/`stop`/`reset`.
- **Atomic credential capture**: one `CRED <urlencoded>` line per submission,
  capturing *all* form fields (not just email/password).
- Open SoftAP + wildcard-DNS captive portal; HTML served from RAM (no SD needed).
- Builds with `arduino-cli` (esp32 core 2.0.17 + async libraries).

### Credits
Derived from the protocol and concept of bigbrodude6119's Evil Portal (MIT).
