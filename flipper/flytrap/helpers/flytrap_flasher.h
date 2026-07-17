#pragma once

#include <furi.h>
#include "../flytrap_uart.h"

// Progress callback, invoked from the flashing worker thread. `stage` is a short
// label ("Connecting", the image filename, "Done"); pct is 0..100 for the current
// image. It must only signal the GUI (post an event), not block.
typedef void (*FlytrapFlashProgress)(
    void* ctx,
    uint8_t img_index,
    uint8_t img_count,
    uint8_t pct,
    const char* stage);

// Read the manifest (a "flash.txt" of "<offset> <filename>" lines, filenames
// relative to the manifest's folder), lend the serial line from `uart`, connect
// to the ESP ROM bootloader, and flash each image with MD5 verify. Blocking —
// call from a worker thread. Returns true on success; on failure writes a short
// reason into `err`. The board must already be in download mode.
bool flytrap_flasher_run(
    FlytrapUart* uart,
    const char* manifest_path,
    FlytrapFlashProgress cb,
    void* cb_ctx,
    char* err,
    size_t err_size);
