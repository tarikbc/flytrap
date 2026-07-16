#include "flytrap_storage.h"
#include "../flytrap_i.h"

#include <storage/storage.h>

void flytrap_storage_ensure_dirs(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, FLYTRAP_DATA_DIR);
    storage_simply_mkdir(storage, FLYTRAP_PORTALS_DIR);
    storage_simply_mkdir(storage, FLYTRAP_LOGS_DIR);
    furi_record_close(RECORD_STORAGE);
}

void flytrap_storage_load_config(FlytrapApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, FLYTRAP_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[FLYTRAP_SSID_MAX];
        size_t n = storage_file_read(file, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        for(size_t i = 0; i < n; i++) {
            if(buf[i] == '\n' || buf[i] == '\r') {
                buf[i] = '\0';
                break;
            }
        }
        if(strlen(buf) > 0) {
            furi_string_set(app->ssid, buf);
            ok = true;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!ok) furi_string_set(app->ssid, "Free WiFi");
}

void flytrap_storage_save_config(FlytrapApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FLYTRAP_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* s = furi_string_get_cstr(app->ssid);
        storage_file_write(file, s, strlen(s));
        storage_file_write(file, "\n", 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool flytrap_storage_read_html(const char* path, uint8_t** out_buf, size_t* out_size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    uint8_t* buf = NULL;
    size_t size = 0;
    bool ok = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t fsize = storage_file_size(file);
        if(fsize > FLYTRAP_HTML_MAX) fsize = FLYTRAP_HTML_MAX;
        size = (size_t)fsize;
        buf = malloc(size + 1);

        size_t total = 0;
        while(total < size) {
            size_t chunk = size - total;
            if(chunk > UINT16_MAX) chunk = UINT16_MAX;
            size_t rd = storage_file_read(file, buf + total, chunk);
            if(rd == 0) break;
            total += rd;
        }
        buf[total] = '\0';
        size = total;
        ok = (size > 0);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        *out_buf = buf;
        *out_size = size;
    } else if(buf) {
        free(buf);
    }
    return ok;
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
        storage_file_write(file, tag, strlen(tag));
        storage_file_write(file, ": ", 2);
        storage_file_write(file, text, strlen(text));
        storage_file_write(file, "\n", 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
