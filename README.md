# Flytrap

A from-scratch **captive-portal ("evil portal") suite for the Flipper Zero + ESP32-S2
WiFi dev board** — built as a hands-on way to *learn how captive portals actually work*.

<p align="center">
  <img src="docs/img/dashboard.png" width="320" alt="Flytrap dashboard">
</p>

Most "sign in to WiFi" pages you've seen at cafés and airports are captive portals.
Flytrap is a small, readable implementation of the same idea — an open access point that
serves a login page and shows you what gets submitted — so you can see every moving part.
It's tuned for the **official Flipper WiFi dev board (ESP32-S2)**, which has no SD card and
trips up heavier tools.

## What you'll learn

- How a captive portal captures a device: **open AP + wildcard DNS + catch-all web server + a form**.
- How a Flipper app and an ESP32 cooperate over a **UART** link (with a documented protocol).
- How a real Flipper app is structured: scenes, views, a background UART worker, SD storage.

Start with **[docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md)** for the concepts, and
**[docs/PROTOCOL.md](docs/PROTOCOL.md)** for the exact serial commands.

## ⚠️ Responsible use

Flytrap creates a fake open Wi-Fi network that serves a login page and captures whatever is
submitted. It exists to **teach how these attacks work so they can be defended against**.

- **Intended for:** students, security researchers, and network admins testing **their own**
  equipment or systems they are **explicitly authorized** to test.
- **The rule:** only run it on networks and devices you **own** or have **written permission**
  to test. Operating a fake AP to capture other people's credentials without consent is
  **illegal** in most jurisdictions.
- The bundled portal pages are **simulated templates for education only** — not affiliated
  with any brand.
- The authors accept no liability for misuse. You are responsible for how you use this.

If that's not your use case, this isn't the project for you.

## Features

- On-device **portal picker**, **SSID** entry, and start/stop — all from the Flipper.
- Live **dashboard**: broadcasting status, credential + client counters.
- **Captures** as a browsable list → detail, with fields **url-decoded** and readable;
  everything also logged to `capture_<N>.txt` on the SD card.
- **Capture alerts** (haptic / beep / LED) with a **Settings** screen to toggle them.
- A **Console** view showing the raw serial protocol live.
- Lightweight enough to run reliably on the SD-less ESP32-S2 dev board.

## Hardware

- **Flipper Zero** (developed on **Momentum** firmware; other forks work with a matching
  `ufbt` SDK).
- **Official Flipper WiFi Dev Board (ESP32-S2)** — or any generic ESP32-S2. It mounts on the
  Flipper's GPIO header, which wires the two together over UART.

## Install & flash

Flytrap is **two parts** — flash the firmware onto the ESP32, install the app on the Flipper.

**1. ESP32-S2 firmware** — put the board in download mode (hold **BOOT**, tap **RESET**,
release **BOOT**) and flash the four images (see [tools/README.md](tools/README.md)):

```sh
esptool --chip esp32s2 --before default-reset --after hard-reset write-flash -z \
  0x1000  esp32/flytrap-fw/build/flytrap-fw.ino.bootloader.bin \
  0x8000  esp32/flytrap-fw/build/flytrap-fw.ino.partitions.bin \
  0xe000  <esp32-core>/tools/partitions/boot_app0.bin \
  0x10000 esp32/flytrap-fw/build/flytrap-fw.ino.bin
```

**2. Flipper app** — build the fap and copy it + the portals to the SD card:

```sh
cd flipper/flytrap && ufbt            # -> dist/flytrap.fap
python3 tools/deploy-to-flipper.py --port /dev/cu.usbmodemflip_XXXX
```

Prefer to build from clean? See [tools/README.md](tools/README.md) and the CI workflow.

## Usage

On the Flipper: **Apps → GPIO → [ESP32] Flytrap**.

1. **Set SSID** — the name of the fake network.
2. **Select Portal** — pick an HTML page from `portals/`.
3. **Start Portal** — the ESP begins broadcasting; the dashboard shows **● Broadcasting**.
4. Connect a **test** device — the captive page pops up; submit to see a capture.
5. **Captures** → browse the list, open one for the decoded fields (Prev/Next to page).

| Screen | Buttons |
|---|---|
| Menu / lists | **↑/↓** move · **OK** select · **←(Back)** back/exit |
| Dashboard | **←** Captures · **→** Console · **Back** to menu |
| Capture detail | **↑/↓** scroll · **←** Prev · **→** Next |
| Settings | **←/→** toggle a value |

<p align="center">
  <img src="docs/img/captures.png" width="240" alt="Captures list">
  <img src="docs/img/console.png" width="240" alt="Console">
</p>

## Portal templates

A portal is just an HTML file with a `<form>`. Drop `.html` files in
`/ext/apps_data/flytrap/portals/` on the SD card and pick one in the app. If a template
contains the token `{{SSID}}`, Flytrap replaces it with the configured network name before
serving. Please keep bundled templates labeled as educational simulations.

## Documentation

- **[HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md)** — captive portals explained (and how to defend).
- **[PROTOCOL.md](docs/PROTOCOL.md)** — the Flipper↔ESP serial command reference.
- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** — how the code is organized.

## Troubleshooting

- **Portal never starts / stuck "Starting…":** the ESP board must be seated on the GPIO
  header and running Flytrap firmware. Open **Console** to watch the handshake
  (`STATUS html_ok → ap_ok → portal_up`).
- **Flipper CLI unresponsive over USB:** hard-reboot the Flipper (hold **←/LEFT + Back** ~5s).
- **ESP32-S2 flash drops mid-erase:** don't use `--erase-all` — the S2's native USB
  disconnects during a full chip erase.

## Contributing

Issues and PRs welcome. Keep the app lightweight (it targets an SD-less board), run the
build (`ufbt` for the app, `arduino-cli` for the firmware — see the CI workflow), and keep
the responsible-use framing intact.

## Credits

- Protocol and concept derive from
  [bigbrodude6119/flipper-zero-evil-portal](https://github.com/bigbrodude6119/flipper-zero-evil-portal) (MIT).
- UI patterns informed by the GhostESP and Marauder Flipper apps.

## License

MIT — see [LICENSE](LICENSE).
