#include "flytrap_format.h"

static int hexval(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Replace control bytes (incl. ESC 0x1B, NUL, CR/LF) with '.' so an adversarial
// captured value can't inject text-scroll formatting markers or break layout.
static void sanitize(FuriString* s) {
    size_t n = furi_string_size(s);
    for(size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)furi_string_get_char(s, i);
        if(c < 0x20 || c == 0x7F) furi_string_set_char(s, i, '.');
    }
}

static bool url_unreserved(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

void flytrap_url_encode(const char* in, FuriString* out) {
    static const char hex[] = "0123456789ABCDEF";
    furi_string_reset(out);
    for(size_t i = 0; in[i] != '\0'; i++) {
        unsigned char c = (unsigned char)in[i];
        if(url_unreserved(c)) {
            furi_string_push_back(out, (char)c);
        } else {
            furi_string_push_back(out, '%');
            furi_string_push_back(out, hex[c >> 4]);
            furi_string_push_back(out, hex[c & 0xF]);
        }
    }
}

void flytrap_url_decode(const char* in, FuriString* out) {
    furi_string_reset(out);
    for(size_t i = 0; in[i] != '\0'; i++) {
        char c = in[i];
        if(c == '+') {
            furi_string_push_back(out, ' ');
        } else if(c == '%' && in[i + 1] && in[i + 2]) {
            int hi = hexval(in[i + 1]);
            int lo = hexval(in[i + 2]);
            if(hi >= 0 && lo >= 0) {
                furi_string_push_back(out, (char)((hi << 4) | lo));
                i += 2;
            } else {
                furi_string_push_back(out, c);
            }
        } else {
            furi_string_push_back(out, c);
        }
    }
}

// Split raw at position i into the next key/value (advances i past the '&').
static void next_field(const char* raw, size_t* i, FuriString* key, FuriString* val) {
    furi_string_reset(key);
    furi_string_reset(val);
    while(raw[*i] && raw[*i] != '=' && raw[*i] != '&') furi_string_push_back(key, raw[(*i)++]);
    if(raw[*i] == '=') {
        (*i)++;
        while(raw[*i] && raw[*i] != '&') furi_string_push_back(val, raw[(*i)++]);
    }
    if(raw[*i] == '&') (*i)++;
}

void flytrap_kv_pretty(const char* raw, FuriString* out) {
    furi_string_reset(out);
    FuriString* key = furi_string_alloc();
    FuriString* val = furi_string_alloc();
    FuriString* dkey = furi_string_alloc();
    FuriString* dval = furi_string_alloc();

    size_t i = 0;
    while(raw[i] != '\0') {
        next_field(raw, &i, key, val);
        if(furi_string_empty(key)) continue;
        flytrap_url_decode(furi_string_get_cstr(key), dkey);
        flytrap_url_decode(furi_string_get_cstr(val), dval);
        sanitize(dkey);
        sanitize(dval);
        // \e# = bold until newline; value on its own indented line
        furi_string_cat_printf(
            out, "\e#%s:\n  %s\n", furi_string_get_cstr(dkey), furi_string_get_cstr(dval));
    }

    furi_string_free(key);
    furi_string_free(val);
    furi_string_free(dkey);
    furi_string_free(dval);
}

void flytrap_kv_get_ip(const char* raw, char* out, size_t out_size) {
    out[0] = '\0';
    FuriString* key = furi_string_alloc();
    FuriString* val = furi_string_alloc();
    FuriString* dkey = furi_string_alloc();
    FuriString* dval = furi_string_alloc();
    size_t i = 0;
    while(raw[i] != '\0') {
        next_field(raw, &i, key, val);
        flytrap_url_decode(furi_string_get_cstr(key), dkey);
        if(furi_string_cmp_str(dkey, "ip") == 0) {
            flytrap_url_decode(furi_string_get_cstr(val), dval);
            sanitize(dval);
            strncpy(out, furi_string_get_cstr(dval), out_size - 1);
            out[out_size - 1] = '\0';
            break;
        }
    }
    furi_string_free(key);
    furi_string_free(val);
    furi_string_free(dkey);
    furi_string_free(dval);
}
