# Flytrap serial protocol

Flytrap is two programs that talk over a **UART** link: the Flipper app and the
ESP32-S2 firmware. This is the wire protocol between them — the part almost no
captive-portal project documents.

- **Link:** the ESP32's `UART0` (hardware serial) wired to the Flipper's GPIO
  header (pins 13/14). On the official WiFi devboard this wiring is built in.
- **Baud:** `115200`, 8N1.
- **Framing:** line-oriented ASCII (`\n`-terminated), with **one binary exception**
  (`sethtml`, below).
- **Direction:** "→ ESP" is sent by the Flipper app; "→ Flipper" is sent by the firmware.

## Flipper → ESP (commands)

| Command | Payload | Meaning |
|---|---|---|
| `sethtml <N>\n` | then exactly **N raw bytes** of HTML | Load the portal page into ESP RAM. Length-prefixed so the HTML can contain any bytes (including newlines) without breaking framing. `N` ≤ `MAX_HTML_SIZE` (48000). |
| `setap <ssid>\n` | — | Set the open access-point name (SSID ≤ 32 chars). |
| `start\n` | — | Bring up the SoftAP + captive DNS + web server. |
| `stop\n` | — | Tear down the AP (keeps the UART/firmware alive). |
| `reset\n` | — | Reboot the ESP (used on app exit). |

## ESP → Flipper (events)

All line-oriented. The Flipper parser matches on the leading token.

| Line | Meaning |
|---|---|
| `STATUS boot` | Firmware started (or rebooted). |
| `STATUS html_ok` | HTML received and stored. |
| `STATUS ap_ok` | SSID set. |
| `STATUS portal_up ip=<ip>` | AP is broadcasting; `<ip>` is the portal's IP. |
| `STATUS stopped` | AP torn down. |
| `STATUS html_err` | Rejected `sethtml` (bad/oversized length). |
| `HIT mac=<AA:BB:...>` | A station joined the AP (its MAC). |
| `BYE mac=<AA:BB:...>` | A station left the AP — the Flipper drops it from the live client list. |
| `IP mac=<AA:BB:...> ip=<ip>` | DHCP assigned a station its IP (MAC resolved from the soft-AP table). Falls back to `IP ip=<ip>` when the MAC can't be resolved; the Flipper then pairs it to the most recent client without an IP. |
| `CRED ip=<ip>&<field>=<val>&...` | A form was submitted — the client IP plus every submitted field, url-encoded. One line per submission. |

## Handshake

The Flipper drives the sequence and waits for each ack, so a slow HTML transfer
can't race the next command:

```
Flipper                              ESP
  |  sethtml <N>\n + <N bytes>  ----> |   (store HTML)
  |  <---- STATUS html_ok             |
  |  setap <ssid>\n            ----> |   (set SSID)
  |  <---- STATUS ap_ok               |
  |  start\n                   ----> |   (SoftAP + DNS + HTTP up)
  |  <---- STATUS portal_up ip=...    |
  |                                   |
  |  <---- HIT mac=...      (a phone joins)
  |  <---- IP mac=... ip=...(DHCP leases it an address)
  |  <---- CRED ip=...&...  (a form is submitted)
  |  <---- BYE mac=...      (the phone leaves)
  |                                   |
  |  stop\n / reset\n          ----> |   (tear down / reboot)
```

If the ESP emits `STATUS boot` mid-session (it rebooted), the Flipper re-sends
`sethtml` automatically to recover.

## Notes

- The ESP holds the portal HTML in **RAM only** — nothing is written to ESP
  storage, which is why Flytrap works on the SD-less official devboard: the
  Flipper streams the page over UART each time.
- `CRED` values are url-encoded (`%XX`, `+`); the Flipper decodes them for display.
- The protocol is intentionally human-readable — open the **Console** screen in
  the app (or a serial terminal at 115200) to watch it live.
