# Changelog

All notable changes to Flytrap are documented here.

## [0.3.0] — unreleased

Interface polish, structured captures, docs, and an important bug fix.

### Fixed
- **App no longer hangs on "loading" when closed.** The UART worker could block posting
  events to a stopped dispatcher during teardown; it's now stopped before the dispatcher
  is freed (with a `closing` guard). Verified on hardware with a live portal.

### Flipper app
- **Refined dashboard** — status tokens mapped to clean words with a `●` indicator
  (`portal_up ip=…` → **Broadcasting**), evenly-aligned rows, buttons on left/right.
- **Structured captures** — browse a **list → detail**; fields are **url-decoded** and shown
  as readable `key: value` lines. The list **live-updates** as captures arrive, and detail
  has **Prev/Next** paging.
- **Settings screen** — toggle the capture alert's **Vibration** and **Sound** (persisted).
- **Console / log viewer** use a formatted scrolling text widget; a custom app icon.
- Hardening: decoded capture fields are sanitized (no text-formatting injection), IP decode
  is consistent, and the legacy `u:`/`p:` path is escaped.

### Docs
- Educational README (responsible-use, hardware, two-part install, button map, screenshots)
  plus `docs/HOW-IT-WORKS.md`, `docs/PROTOCOL.md`, and `docs/ARCHITECTURE.md`.

## [0.2.0] — unreleased

Second feature drop, all reviewed with an adversarial multi-agent pass before
first hardware run.

### Flipper app
- **Scrollable captures** and a **raw serial console** — a shared, scrollable
  `TextBox` view (opened from the dashboard's Captures / Console buttons), each a
  stable snapshot so scrolling isn't yanked to the bottom.
- **View Logs** — browse and open past `capture_<N>.txt` files on-device.
- **Capture alerts** — haptic + LED + beep on each captured credential.
- **Persistent-session menu** — the portal keeps running across the menu and
  sub-views; the menu shows the current SSID + selected portal and a Stop item.
- **FlipperFormat config** — persists the SSID *and* the last-selected portal.
- **RTC timestamps** on every capture (screen + log).
- **SSID templating** — `{{SSID}}` in a portal is replaced with the configured AP name.

### ESP32-S2 firmware (protocol v2.1)
- `HIT mac=<addr>` — includes the joining station's MAC.
- `CRED ip=<addr>&<fields>` — includes the requester's IP.
- Serves a believable **success page** after a submission.
- Serialized protocol output with a mutex (no interleaving across the loop /
  WiFi-event / async-server tasks) and a defensive `sethtml` length cap.

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
