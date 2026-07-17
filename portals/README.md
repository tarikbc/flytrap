# Portal templates

These are **simulated login pages for education only** — they mimic the *look* of common
sign-in screens to demonstrate how a captive portal collects form input. They are **not
affiliated with or endorsed by any brand**, and they contain no real service integration.

Use them only on networks and devices you own or are explicitly authorized to test. See the
project's [Responsible Use](../README.md#-responsible-use) section.

## `social.html`

A single self-contained portal that opens on a **picker** and sends the guest to the
respective brand login — **Google**, **Discord**, or **X** — each a distinct, light-mode
recreation. It also has:

- Real in-page **Terms** and **Privacy** screens.
- **Forgot password / Create account / Register / Sign up** links that show a *"No internet
  connection — connect to the network first"* notice (a captive portal has no internet, so
  those links can't work — which is exactly the point).
- An **Educational demo — do not enter real credentials** disclaimer in every footer.

**Fully offline.** The brand font is an embedded subset of [Inter](https://rsms.me/inter/)
(OFL) as a base64 `@font-face`, so the page makes **no network requests**. Whichever social
button the guest picks, the login form submits `username`, `password`, and a `service` field,
which Flytrap captures as `key: value` pairs.

## Making your own

A portal is just an HTML file with a `<form>`. When it's submitted, every field is captured.
Two conventions Flytrap understands:

- The form's fields become the captured `key: value` pairs.
- The token `{{SSID}}` anywhere in the page is replaced with the configured network name
  before the page is served.

Keep the file **under `MAX_HTML_SIZE` (48000 bytes)** — the whole page is streamed into the
ESP32's RAM buffer. Prefer system fonts or a small embedded subset over remote font/CSS
links, since the portal runs on a network with no internet access.

Copy `.html` files here (and onto the SD card at `/ext/apps_data/flytrap/portals/`), then
pick one in the app.
