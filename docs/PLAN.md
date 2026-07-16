# Flytrap — a from-scratch Evil Portal suite for Flipper Zero + ESP32-S2

## Context

The user has the official Flipper WiFi devboard (**ESP32-S2, no SD slot, no screen**) on a Flipper running **Momentum (mntm-012, Target 7, API 87.1)**. Over this session we established the hard reality of this hardware:

- **bigbrodude Evil Portal** works but is bare-bones: serves a single `index.html`, SSID only via a text file, no on-device portal picker, no live credential view, and several latent bugs.
- **GhostESP** has the nice UX but **OOM-crashes** on this Flipper — it eagerly allocates ~34 submenus + ~39 views + a ~16 KB log buffer (~tens of KB standing), and there's near-zero RAM headroom.
- **Bruce** doesn't support ESP32-S2; **Marauder's** evil portal needs an SD card the board lacks.

So nothing off-the-shelf fits well. The decision: **build a purpose-made suite, "Flytrap," from scratch** — a lightweight Flipper app (won't OOM) with the good UX (on-device picker, SSID entry, live captured credentials) **plus a rewritten ESP32-S2 firmware** that fixes the reference firmware's real protocol limits. Ship it as a public repo. Chosen scope: **app + firmware, full v1** (build everything, then first on-device test).

Outcome: a reliable, feature-complete captive-portal tool tuned to this exact board, and a clean open-source project to grow.

## Deliverables (public repo `flytrap`)

```
flytrap/
  flipper/flytrap/          # the .fap app (built with ufbt, Momentum SDK)
  esp32/flytrap-fw/         # rewritten ESP32-S2 Arduino firmware
  portals/                  # bundled HTML portals (our 29 + revamped)
  tools/                    # deploy/flash scripts (reuse this session's flipper.py, esptool flow)
  README.md  LICENSE  CHANGELOG.md  .gitignore  .github/workflows/build.yml
```

Reference clones already local in scratchpad (patterns to mirror, not copy wholesale):
`scratchpad/flipper-zero-evil-portal/…` (reference app + `esp32/EvilPortal/EvilPortal.ino`), `scratchpad/ghost_esp_app/…` (picker + text-input patterns).

## Flytrap wire protocol v2 (we own both ends now)

Fixes the reference's `Serial.readString()` payload-splitting, unbounded copies, and fragile `u:`/`p:` pairing.

**Flipper → ESP (newline-framed; HTML is length-prefixed so it can't split):**
- `sethtml <N>\n` followed by exactly **N raw bytes** of HTML. ESP reads N bytes in a loop (no timeout guessing). Lifts the ~11 KB practical cap → up to a new `MAX_HTML_SIZE` (e.g. 24000, RAM permitting).
- `setap <ssid>\n` (SSID ≤ 32, bounds-checked).
- `start\n` — explicit start (no more implicit auto-start ambiguity).
- `stop\n` / `reset\n` — stop portal / reboot ESP.

**ESP → Flipper (line-oriented ASCII, `\n`-terminated, easy to parse):**
- `STATUS <token>` — e.g. `html_ok`, `ap_ok`, `portal_up ip=<ip>`, `stopped`.
- `HIT` — a client hit the portal (increment counter).
- `CRED <urlencoded>` — **atomic**, captures **all** submitted form fields, not just email/password: e.g. `CRED email=a%40b.com&password=hunter2`. One line = one submission; no pairing race.

Flipper parser also tolerates the legacy `u:`/`p:` lines so it still works against the old firmware if needed.

## Flipper app architecture (`flipper/flytrap/`)

Lean by design — the anti-GhostESP. **4 scenes, 4 small views**; the picker is a blocking dialog (zero standing view cost).

```
flytrap/
  application.fam            # appid="flytrap", name="[ESP32] Flytrap", requires=["gui","dialogs"], stack_size=2*1024, fap_category="GPIO"
  flytrap.c / flytrap_i.h    # alloc/free, dispatcher wiring, app struct, #defines
  flytrap_uart.c/.h          # furi_hal_serial layer (ported) — see expansion note below
  flytrap_custom_event.h
  helpers/
    flytrap_storage.c/.h     # HTML read (corrected), config load/save, creds log
    flytrap_parser.c/.h      # RX line assembler + STATUS/HIT/CRED (+legacy u:/p:) parsing
  scenes/
    flytrap_scene.c/.h + flytrap_scene_config.h   # X-macro tables (mirror reference verbatim)
    flytrap_scene_main_menu.c     # Submenu: Select Portal / Set SSID / Start / Stop / Raw Log / About
    flytrap_scene_ssid_input.c    # TextInput → config
    flytrap_scene_portal_picker.c # dialog_file_browser_show(app->dialogs, …, ".html")
    flytrap_scene_live.c          # start handshake + Widget (captures) / TextBox (raw)
  icons/flytrap_10px.png
```

**Views:** `Submenu` (menu), `TextInput` (SSID), `Widget` (live captures: SSID, state, client count, capture count, last N), `TextBox` (raw status log, 2 KB store). Portal picker = `dialog_file_browser_show` (blocking `DialogsApp`, no view).

**App struct (lean):** dispatcher/scene_manager/dialogs, the 4 view handles, `console_store` (2 KB reserve), `EvilPortalUart* uart`, `FuriString ssid/portal_path`, `char ssid_buf[33]`, `FuriString line_acc`, a fixed **8-slot capture ring** `{char kv[96];}` (~768 B, holds urlencoded fields), `cap_head/cap_count/client_count`, and handshake flags. Steady-state heap < ~10 KB.

**Critical UART detail (already learned the hard way):** acquiring the GPIO USART requires
`expansion_disable(furi_record_open(RECORD_EXPANSION))` **before** `furi_hal_serial_control_acquire()`, and `expansion_enable()` + close **after** release — otherwise `furi_check` crashes on start. Reuse the ported `flytrap_uart.c` pattern.

**Paths:** data dir `/ext/apps_data/flytrap/`; portals `/ext/apps_data/flytrap/portals/`; config `/ext/apps_data/flytrap/config.txt` (FlipperFormat: `SSID: …`); creds `/ext/apps_data/flytrap/logs/capture_<N>.txt`.

**Data flow:** pick portal (dialog) → set SSID (TextInput→config) → Start (Live scene): correct chunked HTML read into one `size+1` buffer (NUL-terminated, base pointer freed), `sethtml <N>\n`+bytes, wait `STATUS html_ok`, `setap <ssid>\n`, `start\n`; parser updates state, counts `HIT`, stores `CRED` into ring + appends to log (open-append); Stop/exit → `reset\n`, flush. GUI touched only via `view_dispatcher_send_custom_event` from the RX worker.

**Reference bugs we fix:** free the base pointer (not advanced cursor); NUL-terminate reads; use `furi_string_size()` not utf8_length for log byte count; free the sequential path; drop the dead StartPortal view and broken `help` guard; guard `text_input_show_illegal_symbols` behind a Momentum `#ifdef`.

## ESP32-S2 firmware rewrite (`esp32/flytrap-fw/`)

Arduino sketch (libs: `ESPAsyncWebServer`, `AsyncTCP`, `DNSServer`, `WiFi`). Based on `EvilPortal.ino` but implementing protocol v2:
- **Length-prefixed HTML read** (read exactly N bytes) → no `readString()` splitting; raise `MAX_HTML_SIZE`.
- **Bounded copies** into `index_html`/`apName` (clamp to capacity — fixes overflow).
- Open SoftAP + wildcard DNS (`dnsServer.start(53,"*",AP_IP)`) + catch-all captive handler (unchanged, works well).
- `/get` (and `/post`) handler iterates **all** params → emits one `CRED <urlencoded>` line.
- Emits `STATUS`/`HIT` per protocol. Explicit `start`/`stop`/`reset`.
- No onboard storage (HTML stays in RAM streamed from Flipper) — confirmed the S2 board needs no SD.

Build for the official devboard: `arduino-cli compile --fqbn esp32:esp32:esp32s2 …` (or PlatformIO), producing bootloader/partitions/boot_app0/app bins (or a merged image). Flash with **esptool at documented offsets, WITHOUT `--erase-all`** (S2 native-USB drops the CDC on full erase — learned this session).

## Tooling & build

- **Flipper app:** `ufbt` with the Momentum SDK already set up at `scratchpad/ufbt` (`UFBT_HOME`); `ufbt` builds `dist/flytrap.fap` (must show Target 7, API 87.1).
- **ESP firmware:** `arduino-cli` + esp32 core for `esp32s2` (install if missing), or PlatformIO. Flash via the esptool flow proven this session.
- **Deploy to Flipper:** reuse `scratchpad/flipper.py` (`storage write_chunk` + md5 verify) to push the fap to `/ext/apps/GPIO/flytrap.fap` and portals to `/ext/apps_data/flytrap/portals/`. Requires the Flipper at its desktop with a responsive CLI (hard-reboot LEFT+BACK if the CLI hangs).

## Build sequence (full v1, ordered so each stage is buildable)

1. **Repo scaffold + skeleton app** — dirs, `application.fam`, `flytrap_i.h`, X-macro scene files, main menu with stub items, alloc/free, UART init/free (expansion dance) + TX helper. Builds, launches, exits clean, `reset` on exit, no crash.
2. **Storage + config + picker** — `flytrap_storage.c` (corrected HTML read, config load/save), portal picker dialog, SSID TextInput scene wired to config.
3. **Start/stop + handshake** — Live scene: length-prefixed `sethtml`, `STATUS html_ok` wait, `setap`, `start`; raw status in TextBox; Stop=`reset`.
4. **Parser + live captures view** — `flytrap_parser.c` (STATUS/HIT/CRED + legacy), Widget with counts + last N captures, append to `logs/capture_N.txt`.
5. **ESP firmware v2** — rewrite `flytrap-fw`, build for esp32s2, produce bins.
6. **Polish** — start guardrails + error dialogs, HTML-size warning, About, log rotation, icon, README/CHANGELOG, CI workflow.

## Repo scaffolding & licensing

- Read `scratchpad/flipper-zero-evil-portal/LICENSE.txt` and **honor/attribute** the base project (protocol + concept derive from bigbrodude's Evil Portal). Match a compatible license; add attribution in README + headers.
- README: what it is, hardware (official S2 devboard), install (flash firmware + install fap), usage (pick → SSID → start → captures), **legal/authorized-use warning**, credits.
- `.github/workflows/build.yml`: build the fap with ufbt and the firmware with arduino-cli on push.
- Creating/pushing the GitHub repo is an external action — do it only on explicit go-ahead (offer `gh repo create`).

## Verification (end-to-end on real hardware)

1. `ufbt` builds `flytrap.fap` cleanly at **Target 7 / API 87.1** (APPCHK).
2. Flash Flytrap firmware to the S2 board (esptool, no erase-all); confirm **hash verified**.
3. Deploy fap + portals over USB (md5-verified); confirm `[ESP32] Flytrap` appears under Apps → GPIO.
4. On device: **Set SSID**, **Select Portal** (browse the 29), **Start** — confirm an **open** AP with that SSID appears on a phone and the captive page loads (the real bug we chased: portal actually broadcasts).
5. Submit test creds on the captive page → **live captures view increments**, entry shows, and `logs/capture_N.txt` is written. Verify the log over USB.
6. **Stop** → AP disappears; app exits without OOM/`furi_check` (the two crash classes we hit) across repeated start/stop cycles.
7. Capture a Flipper **screenshot** via the RPC tool (`scratchpad/screenshot.py`) at the live-captures screen as evidence.
