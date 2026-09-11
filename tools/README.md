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

The app flashes the board itself over the GPIO UART — no computer, no esptool. The
firmware images are **bundled inside the fap** (`fap_file_assets`) and extracted to
`/ext/apps_assets/flytrap/firmware/flytrap/` on first launch, so there's nothing to
copy to the SD. The bundle is a `flash.txt` manifest plus the `.bin` images it lists:

```
# <flash offset>  <file>
0x1000  flytrap-fw.ino.bootloader.bin
0x8000  flytrap-fw.ino.partitions.bin
0xe000  boot_app0.bin
0x10000 flytrap-fw.ino.bin
```

Two ways to trigger it, both flashing that bundled image (no file picker):

- **Start Portal** auto-detects the board via its `PING FTRP <ver>` beacon and, if
  the firmware is missing or older than the app expects, offers **Install** /
  **Update**, then continues to broadcasting once flashed.
- **Reinstall firmware** in the menu flashes it on demand (e.g. to recover).

Either way, **hold BOOT, tap RESET, release BOOT** when prompted — it auto-detects
the board in download mode and flashes. When it finishes, **tap RESET again** to run
the new firmware: the board is still held in the download bootloader (the app can't
drive the S2's reset line), so it won't boot the flashed image on its own. The
on-device "Flashed!" screen says as much; press **Continue** after the reset and it
re-detects the board and carries on. Releases also ship
`flytrap-firmware-bundle.zip` for flashing from a computer with `esptool` instead.

**Other boards (WROOM, C5).** Only the S2 image rides inside the fap, so those two are
flashed from a computer. Each release attaches a single merged image per board, already
containing the bootloader, partition table and boot_app0, so it goes on at `0x0`:

```sh
esptool --chip esp32   --port /dev/ttyUSB0 write-flash 0x0 flytrap-wroom-merged.bin
esptool --chip esp32c5 --port /dev/ttyUSB0 write-flash 0x0 flytrap-c5-merged.bin
```

Once flashed the board beacons `PING FTRP <ver>` like any other, so the Flipper detects
it and runs a session normally. It just cannot install or update it over the GPIO UART.

## Deploy to the Flipper (over USB, no SD removal)

`deploy-to-flipper.py` uploads the fap to `/ext/apps/GPIO/` and the bundled portals
to `/ext/apps_data/flytrap/portals/`, verifying every file by on-device md5. It uses
the Flipper's serial CLI (`storage write_chunk` / `storage md5`). The ESP firmware
rides inside the fap, so it isn't pushed separately.

```sh
python3 tools/deploy-to-flipper.py --port /dev/cu.usbmodemflip_XXXX
```

Then on the Flipper: **Apps → GPIO → [ESP32] Flytrap** → Set SSID → Select Portal → Start Portal.
