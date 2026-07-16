# Portal templates

These are **simulated login pages for education only** — they mimic the *look* of common
sign-in screens to demonstrate how a captive portal collects form input. They are **not
affiliated with or endorsed by any brand**, and they contain no real service integration.

Use them only on networks and devices you own or are explicitly authorized to test. See the
project's [Responsible Use](../README.md#-responsible-use) section.

## Making your own

A portal is just an HTML file with a `<form>`. When it's submitted, every field is captured.
Two conventions Flytrap understands:

- The form's fields become the captured `key: value` pairs.
- The token `{{SSID}}` anywhere in the page is replaced with the configured network name
  before the page is served.

Copy `.html` files here (and onto the SD card at `/ext/apps_data/flytrap/portals/`), then
pick one in the app.
