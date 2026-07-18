# Changelog

All notable changes to Flytrap are documented here.

## [0.5.0] — 2026-07-17

Flash the ESP firmware from the Flipper — no computer.

### Flipper app
- **Flash Firmware** — a new menu item flashes the ESP over the GPIO UART using a
  vendored [esp-serial-flasher](https://github.com/espressif/esp-serial-flasher)
  (Apache-2.0) + a small `furi_hal_serial` port. Pick a firmware bundle from the SD
  (a `flash.txt` manifest of `<offset> <file>`); it **auto-detects** the board in
  download mode (hold BOOT, tap RESET) via the stub loader, streams each image with
  MD5 verify, and reboots the board. Progress + result on-device.
- **Start Portal self-heals** — if the board isn't running Flytrap firmware (no
  beacon), Start now offers **Install firmware**, runs the flasher, and on
  **Continue** returns to the start flow, re-detects the flashed board, and brings
  the portal up. No computer, no separate step.
- Only the **ESP32-S2** stub is compiled in (the full multi-chip stub table would
  balloon the fap's RAM footprint and OOM the app).
- **One-step install** — the portal and firmware bundle are bundled in the fap
  (`fap_file_assets`, extracted to `apps_assets` on launch), so a fresh user just
  installs the `.fap`; the defaults point at the bundled assets.

### Fixed
- **OOM starting the portal.** `furi_string_replace_all` on the ~38 KB portal built
  a second full copy (~2x heap); replaced with an in-place `furi_string_replace_str`
  loop. (Surfaced once the flasher library reduced free heap.)

### Tooling / CI
- `tools/deploy-to-flipper.py` also uploads a firmware bundle to
  `/ext/apps_data/flytrap/firmware/flytrap/` (built images + `boot_app0.bin` +
  `flash.txt`).
- The release workflow attaches a ready-to-flash `flytrap-firmware-bundle.zip`.

## [0.4.1] — 2026-07-17

Fixes for the portal-start flow, all found on hardware.

### Fixed
- **Out-of-memory crash on Start.** The ~38.5 KB portal was read byte-by-byte into a
  growing `FuriString`, whose reallocations briefly held two buffers (~2x peak) and blew
  the heap. The buffer is now reserved once from the file size, so the peak is ~1x.
- **"Detecting board…" screen.** Start paints a status screen before the (possibly
  blocking) board check, so it no longer looks frozen; then **Starting portal…**, or
  **No board detected** if nothing answers.
- **Auto-detect on the No-board screen.** Plugging the board in while "No board detected"
  is shown now auto-continues the start within ~1–3 s (no Back + Start needed).
- **Session ends when the board is unplugged.** Losing the link now ends the session, so
  the menu returns to **Start Portal** instead of still offering Stop/Dashboard/Console
  (updates even while sitting on the menu).
- **No false "Board disconnected" flash** when starting right after a disconnect — stale
  link state is cleared at the start of each run.

## [0.4.0] — 2026-07-17

A single offline social portal, a live clients view, and board-liveness handling.

### Flipper app
- **Live clients** — a real-time list of connected stations (MAC, IP, joined time) with a
  list → detail view, reached from the dashboard (**→ Clients**). Joins/leaves/IP-leases
  update it live, so a device that disconnects is removed and the **Clients** counter is
  accurate (previously it only ever counted up). Console moved to the menu to keep the
  dashboard to two uncrowded buttons (**← Captures · → Clients**).
- **Board-liveness handling** — the ESP beacons `PING` ~every 2s; a 1s tick flags the link
  lost after 5s of silence, so the dashboard shows **Board disconnected** instead of falsely
  claiming **Broadcasting**, and recovers automatically when the board returns. **Start
  Portal** also checks the board is attached first and shows **No board detected** rather
  than hanging on the loading screen.
- **Loading screen** — Start Portal shows a *Starting…* screen while the ~38.5 KB portal
  streams over UART (which blocks the UI ~3s), instead of freezing on the dashboard.

### Firmware
- **Station join/leave/IP events.** The ESP now emits `BYE mac=…` when a station leaves
  and `IP mac=… ip=…` when DHCP leases an address (MAC resolved from the soft-AP table),
  in addition to `HIT`. This is what powers the live client list and accurate counter.
- **`PING` liveness beacon** (~every 2s) so the Flipper can tell the board is still
  attached and flag the link as lost when it isn't.

### Portals
- **Single `social.html` portal replaces the old template pile.** One page with a picker that
  sends the guest to the respective brand login (Google / Discord / X), each a distinct,
  light-mode recreation. Real in-page **Terms** and **Privacy** screens; **Forgot password /
  Register / Sign up** links show a "no internet — connect first" notice. Educational-demo
  disclaimer in every footer and the file header.
- **Offline by design** — the brand font is an embedded subset of Inter (OFL, ~15 KB base64),
  so the page makes **no network requests** (a captive portal has no internet).
- **Bigger portals fit** — `MAX_HTML_SIZE` / `FLYTRAP_HTML_MAX` raised 24000 → **48000** so a
  richer single-file portal streams to the ESP RAM buffer with headroom.

### Docs
- PROTOCOL/ARCHITECTURE/HOW-IT-WORKS document the join/leave/IP/PING events, the live
  clients view, and the loading / board-disconnected states; a tag-triggered release
  workflow builds and publishes the fap + firmware.

## [0.3.0] — 2026-07-16

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
