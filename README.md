# Flytrap

A from-scratch **captive-portal ("evil portal") suite for the Flipper Zero + ESP32-S2 WiFi dev board.**

Flytrap is a lightweight Flipper app plus a rewritten ESP32-S2 firmware. It's built specifically for the **official Flipper WiFi dev board** (ESP32-S2, no SD slot, no screen) — the hardware that heavier tools like GhostESP run out of memory on and that Bruce/Marauder don't fully support for portals.

## What it does

- **On-device portal picker** — browse and select an HTML portal on the Flipper, no cable needed.
- **SSID entry from the UI** — set the fake AP name on-device, persisted to config.
- **Start / stop** the open access point + captive portal.
- **Live captured credentials** — submitted form fields stream back to the Flipper, shown with a running count and saved to a log on the SD card.
- **Small and robust** — a handful of views, lazy allocation, bounded buffers. It won't OOM.

## Status

🚧 **Planning / scaffolding.** The full design lives in [`docs/PLAN.md`](docs/PLAN.md). Implementation follows the phased build sequence there.

## Repo layout

```
flipper/flytrap/      # the Flipper .fap app (built with ufbt, Momentum SDK)
esp32/flytrap-fw/     # rewritten ESP32-S2 Arduino firmware (protocol v2)
portals/              # bundled HTML portals
tools/                # deploy / flash helpers
docs/PLAN.md          # architecture + build plan
```

## Hardware

- Flipper Zero (developed against **Momentum** firmware; other forks should work with a matching `ufbt` SDK)
- Official Flipper **WiFi Dev Board (ESP32-S2)** — or any generic ESP32-S2

## ⚠️ Legal / authorized use only

Flytrap creates a fake open Wi-Fi access point that serves a captive login page and captures whatever is submitted. Run it **only** on networks and devices you own or are explicitly authorized to test (your own lab, a sanctioned penetration test, or security education). Operating a fake AP to capture other people's credentials without consent is illegal in most jurisdictions. You are responsible for how you use this.

## Credits

- Based on the protocol and concept of [bigbrodude6119/flipper-zero-evil-portal](https://github.com/bigbrodude6119/flipper-zero-evil-portal) (MIT).
- UI patterns (file-browser picker, text input) informed by the GhostESP companion app.

## License

MIT — see [`LICENSE`](LICENSE).
