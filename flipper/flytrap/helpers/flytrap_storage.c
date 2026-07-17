#include "flytrap_storage.h"
#include "../flytrap_i.h"

#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

void flytrap_timestamp(FuriString* out) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    furi_string_printf(out, "[%02u:%02u:%02u] ", dt.hour, dt.minute, dt.second);
}

void flytrap_storage_ensure_dirs(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, FLYTRAP_DATA_DIR);
    storage_simply_mkdir(storage, FLYTRAP_PORTALS_DIR);
    storage_simply_mkdir(storage, FLYTRAP_LOGS_DIR);
    furi_record_close(RECORD_STORAGE);
}

void flytrap_storage_load_config(FlytrapApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    bool have_ssid = false;

    if(flipper_format_file_open_existing(ff, FLYTRAP_CONFIG_PATH)) {
        uint32_t ver = 0;
        if(flipper_format_read_header(ff, tmp, &ver)) {
            flipper_format_rewind(ff);
            if(flipper_format_read_string(ff, "SSID", tmp)) {
                furi_string_set(app->ssid, tmp);
                have_ssid = true;
            }
            flipper_format_rewind(ff);
            if(flipper_format_read_string(ff, "PortalPath", tmp)) {
                furi_string_set(app->portal_path, tmp);
            }
            uint32_t v = 0;
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "Sound", &v, 1)) app->sound_on = (v != 0);
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "Vibro", &v, 1)) app->vibro_on = (v != 0);
        }
    }

    flipper_format_free(ff);
    furi_string_free(tmp);
    furi_record_close(RECORD_STORAGE);
    if(!have_ssid) furi_string_set(app->ssid, "Free WiFi");
}

void flytrap_storage_save_config(FlytrapApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_always(ff, FLYTRAP_CONFIG_PATH)) {
        flipper_format_write_header_cstr(ff, "Flytrap Config", 1);
        flipper_format_write_string_cstr(ff, "SSID", furi_string_get_cstr(app->ssid));
        flipper_format_write_string_cstr(ff, "PortalPath", furi_string_get_cstr(app->portal_path));
        uint32_t sound = app->sound_on ? 1 : 0;
        uint32_t vibro = app->vibro_on ? 1 : 0;
        flipper_format_write_uint32(ff, "Sound", &sound, 1);
        flipper_format_write_uint32(ff, "Vibro", &vibro, 1);
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

bool flytrap_storage_read_file(const char* path, FuriString* out, size_t cap) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    furi_string_reset(out);
    size_t total = 0;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Reserve the whole buffer up front. Without this, appending byte-by-byte
        // reallocs the string geometrically, and each grow briefly holds both the
        // old and new buffers — a ~2x peak that OOMs the Flipper on a large (38 KB)
        // portal. The +128 leaves room for {{SSID}} expansion done by the caller.
        uint64_t fsize = storage_file_size(file);
        size_t reserve = (fsize < cap ? (size_t)fsize : cap) + 128;
        furi_string_reserve(out, reserve);
        uint8_t buf[257];
        while(total < cap) {
            size_t want = cap - total;
            if(want > sizeof(buf) - 1) want = sizeof(buf) - 1;
            size_t rd = storage_file_read(file, buf, want);
            if(rd == 0) break;
            // Length-aware append (don't stop at an embedded NUL byte).
            for(size_t i = 0; i < rd; i++) furi_string_push_back(out, (char)buf[i]);
            total += rd;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return total > 0;
}

void flytrap_storage_new_log(FlytrapApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, FLYTRAP_LOGS_DIR);
    FuriString* path = furi_string_alloc();
    uint32_t i = 0;
    do {
        furi_string_printf(path, "%s/capture_%lu.txt", FLYTRAP_LOGS_DIR, (unsigned long)i);
        i++;
    } while(storage_file_exists(storage, furi_string_get_cstr(path)) && i < 100000);
    furi_string_set(app->log_path, path);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
}

void flytrap_storage_append_log(FlytrapApp* app, const char* tag, const char* text) {
    if(furi_string_empty(app->log_path)) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(
           file, furi_string_get_cstr(app->log_path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FuriString* ts = furi_string_alloc();
        flytrap_timestamp(ts);
        storage_file_write(file, furi_string_get_cstr(ts), furi_string_size(ts));
        furi_string_free(ts);
        storage_file_write(file, tag, strlen(tag));
        storage_file_write(file, ": ", 2);
        storage_file_write(file, text, strlen(text));
        storage_file_write(file, "\n", 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
