# Tools

Helper scripts for building, flashing, and deploying Flytrap.

## Prerequisites

- **Flipper app:** [`ufbt`](https://pypi.org/project/ufbt/) with a matching SDK
  (Momentum by default — see the CI workflow for the exact `ufbt update` command).
- **ESP firmware:** [`arduino-cli`](https://arduino.github.io/arduino-cli/) with the
  `esp32:esp32@2.0.17` core and the async libraries (`me-no-dev/AsyncTCP`,
  `me-no-dev/ESPAsyncWebServer`) — see `.github/workflows/build.yml`.
- **Flashing / deploy:** Python 3 with `pyserial` and `esptool`.

## Build

```sh
# Flipper app -> flipper/flytrap/dist/flytrap.fap
cd flipper/flytrap && ufbt

# ESP32-S2 firmware -> esp32/flytrap-fw/build/*.bin
arduino-cli compile --fqbn esp32:esp32:esp32s2:PartitionScheme=huge_app \
  --libraries esp32/libs --output-dir esp32/flytrap-fw/build esp32/flytrap-fw
```

## Flash the ESP32-S2

Put the board in download mode (**hold BOOT, tap RESET, release BOOT**), then flash
the four images without a full chip erase (the S2's native USB drops the CDC during a
whole-chip erase):

```sh
esptool --chip esp32s2 --before default-reset --after hard-reset write-flash -z \
  0x1000  esp32/flytrap-fw/build/flytrap-fw.ino.bootloader.bin \
  0x8000  esp32/flytrap-fw/build/flytrap-fw.ino.partitions.bin \
  0xe000  <esp32-core>/tools/partitions/boot_app0.bin \
  0x10000 esp32/flytrap-fw/build/flytrap-fw.ino.bin
```

## Flash the ESP32-S2 from the Flipper (no computer)

The app can flash the board itself over the GPIO UART — **Apps → GPIO → [ESP32]
Flytrap → Flash Firmware**. It reads a firmware **bundle** from the SD: a folder
under `/ext/apps_data/flytrap/firmware/` containing a `flash.txt` manifest plus the
`.bin` images it lists:

```
# <flash offset>  <file>
0x1000  flytrap-fw.ino.bootloader.bin
0x8000  flytrap-fw.ino.partitions.bin
0xe000  boot_app0.bin
0x10000 flytrap-fw.ino.bin
```

Pick the manifest, then **hold BOOT, tap RESET, release BOOT** — it auto-detects the
board in download mode and flashes. `deploy-to-flipper.py` (below) installs the
Flytrap bundle automatically; releases also ship `flytrap-firmware-bundle.zip` (unzip
its `flytrap/` folder into `/ext/apps_data/flytrap/firmware/`). Starting the portal
without Flytrap firmware also offers to install it, then resumes.

## Deploy to the Flipper (over USB, no SD removal)

`deploy-to-flipper.py` uploads the fap to `/ext/apps/GPIO/`, the bundled portals to
`/ext/apps_data/flytrap/portals/`, and (if the ESP firmware is built) the firmware
bundle to `/ext/apps_data/flytrap/firmware/flytrap/`, verifying every file by
on-device md5. It uses the Flipper's serial CLI (`storage write_chunk` / `storage md5`).

```sh
python3 tools/deploy-to-flipper.py --port /dev/cu.usbmodemflip_XXXX
```

Then on the Flipper: **Apps → GPIO → [ESP32] Flytrap** → Set SSID → Select Portal → Start Portal.
