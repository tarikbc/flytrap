#pragma once

#include <stddef.h>
#include <stdint.h>

// UART rx callback: assembles lines from ESP output and updates app state.
// ctx must be a FlytrapApp*. Runs on the UART worker thread.
void flytrap_parser_feed(uint8_t* buf, size_t len, void* ctx);
