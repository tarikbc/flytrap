#!/bin/sh
# Build the Flipper fap with its bundled assets (portal + ESP firmware).
#
# The fap ships these via fap_file_assets; on first launch the firmware extracts
# them to /ext/apps_assets/flytrap/, so a fresh user gets a working portal and a
# flashable firmware bundle just by installing the .fap — no SD juggling.
#
# Build the ESP firmware first (tools/README) so its images get bundled; without
# them the fap still bundles the portal (on-device flashing just won't have a
# default bundle). The assets dir is generated — it's git-ignored.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
A="$ROOT/flipper/flytrap/assets"

rm -rf "$A"
mkdir -p "$A/portals" "$A/firmware/flytrap"
cp "$ROOT/portals/social.html" "$A/portals/"
cp "$ROOT/esp32/flytrap-fw/flash.txt" "$ROOT/esp32/flytrap-fw/boot_app0.bin" "$A/firmware/flytrap/"

if [ -f "$ROOT/esp32/flytrap-fw/build/flytrap-fw.ino.bin" ]; then
    cp "$ROOT"/esp32/flytrap-fw/build/flytrap-fw.ino.bootloader.bin \
       "$ROOT"/esp32/flytrap-fw/build/flytrap-fw.ino.partitions.bin \
       "$ROOT"/esp32/flytrap-fw/build/flytrap-fw.ino.bin "$A/firmware/flytrap/"
else
    echo "WARN: ESP firmware not built — bundling the portal but not firmware images"
fi

cd "$ROOT/flipper/flytrap"
exec ufbt "$@"
