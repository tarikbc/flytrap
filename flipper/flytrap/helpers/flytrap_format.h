#pragma once

#include <furi.h>

// Decode a url-encoded string (%XX escapes and '+' as space) into `out`.
void flytrap_url_decode(const char* in, FuriString* out);

// Percent-encode `in` (reserved/control bytes) into `out`.
void flytrap_url_encode(const char* in, FuriString* out);

// Turn "k1=v1&k2=v2&..." into readable, decoded, scroll-widget-formatted text:
//   \e#key:\n  value\n   (bold label, indented value) per field.
void flytrap_kv_pretty(const char* raw, FuriString* out);

// Extract the value of key "ip" from a "k=v&..." string into `out` (empty if none).
void flytrap_kv_get_ip(const char* raw, char* out, size_t out_size);
