# How it works

Flytrap is a small, readable implementation of a **captive portal** — the "sign in
to WiFi" page you've seen at airports and cafés — built to teach how the pieces fit
together. This page explains the concepts; the code is deliberately compact so you
can read it end to end.

> ⚠️ Everything here is for learning and **authorized** testing only. See the
> [Responsible Use](../README.md#-responsible-use) section.

## The big picture

```
   phone / laptop            ESP32-S2 (Flytrap fw)          Flipper Zero (Flytrap app)
  ┌──────────────┐   WiFi   ┌────────────────────┐  UART   ┌───────────────────────┐
  │  joins the   │ <──────> │ open AP + DNS + web │ <─────> │ picks portal, sets    │
  │  open AP,    │          │ server (captive)    │  115200 │ SSID, shows captures  │
  │  sees login  │          │ streams creds back  │         │ + logs to SD          │
  └──────────────┘          └────────────────────┘         └───────────────────────┘
```

The **ESP32** does the radio + web work. The **Flipper** is the brains + screen +
storage. They talk over a serial ([UART](PROTOCOL.md)) link.

## What makes a captive portal "capture" you

Four things happen when a device joins the fake network:

1. **An open access point.** The ESP creates a password-less WiFi network
   (`WiFi.softAP`). Anything nearby can join — no credentials needed.

2. **A DNS that always lies.** Normally DNS turns `example.com` into an IP. The ESP
   runs a DNS server configured with a wildcard: **every** lookup, for any domain,
   resolves to the ESP's own IP (`dnsServer.start(53, "*", apIP)`). So the moment a
   phone tries to reach *anything*, it's pointed back at the ESP.

3. **A web server that answers everything.** The ESP's HTTP handler `canHandle()`s
   every request path and returns the **same** portal HTML. Phones detect captive
   portals by quietly requesting a known URL (e.g. Apple's
   `captive.apple.com/hotspot-detect.html`); because that request also lands on our
   catch-all and returns a login page instead of the expected content, the OS pops
   the "Sign in to network" sheet automatically.

4. **A form that phones home.** The portal page is just HTML with a `<form>`. When
   it's submitted, the ESP reads every field, url-encodes them into one
   `CRED ip=...&email=...&password=...` line, and sends it up the UART to the
   Flipper, which decodes and shows it. Then the ESP serves a believable
   "Connecting…" success page so nothing looks off.

That's the whole trick: **open AP + wildcard DNS + catch-all web server + a form.**
There's no exploit — it works because joining an open network means trusting
whatever that network says.

## Why two devices, and why this board is special

Most ESP32 captive-portal tools store the portal HTML (and captured creds) on a
**microSD card** attached to the ESP. The **official Flipper WiFi devboard has no SD
slot**. Flytrap works around that by keeping the ESP dumb: the Flipper **streams the
HTML over UART** (`sethtml <N>` + bytes) each time you start, and captures come
**back** over UART to be logged on the *Flipper's* SD. This is why the firmware never
touches any filesystem — see [PROTOCOL.md](PROTOCOL.md).

## The Flipper app side

The app is a standard Flipper `ViewDispatcher` + `SceneManager` structure:

- **Main menu** → start/stop the portal, pick a portal HTML, set the SSID, view logs, console.
- **Dashboard** (a `Widget`) → live status (with a *Starting…* screen and a *Board
  disconnected* state when the ESP's beacon stops), credential + client counters.
- **Captures** → a list of captures → a detail screen that url-decodes each field.
- **Clients** → who's connected right now (MAC, IP, joined time); updates live as
  stations join, get a DHCP lease, and leave.
- **Console** → the raw serial stream, exactly as in [PROTOCOL.md](PROTOCOL.md).

A background thread reads UART bytes; all parsing happens on the GUI thread (so there
are no data races), and captures are both shown and appended to `capture_<N>.txt` on
the SD card.

## How to defend against this

The point of understanding the attack is building the defense:

- **Don't submit credentials on open networks.** A real service uses HTTPS with a
  valid certificate; a captive portal serving a "login" over plain HTTP for an
  unrelated site is the tell.
- **Treat auto-popping "sign in" pages with suspicion** — never reuse a real password
  there.
- Operators can use **802.11w / secure onboarding, WPA2/3, and client isolation**, and
  educate users that legitimate captive portals never ask for email-provider or
  social passwords.

## Read next

- [PROTOCOL.md](PROTOCOL.md) — the exact Flipper↔ESP serial commands.
- [ARCHITECTURE.md](ARCHITECTURE.md) — how the code is organized.
