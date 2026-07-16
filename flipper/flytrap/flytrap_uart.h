#pragma once

#include <furi.h>

typedef struct FlytrapUart FlytrapUart;

// Called from the UART worker thread whenever new RX bytes are available.
// It must only signal (e.g. post a ViewDispatcher custom event) — the actual
// bytes are drained on the consumer thread via flytrap_uart_rx(). The context
// is fixed at init and never changes, so there is no set/clear race.
typedef void (*FlytrapUartNotify)(void* ctx);

FlytrapUart* flytrap_uart_init(uint32_t baudrate, FlytrapUartNotify notify, void* notify_ctx);

// Two-phase teardown so the caller can stop the RX worker (no more notify calls)
// BEFORE freeing the ViewDispatcher, then still tx a final command, then release.
void flytrap_uart_stop_rx(FlytrapUart* uart); // stop async RX + join worker
void flytrap_uart_deinit(FlytrapUart* uart); // release serial + free (call last)

// Drain up to maxlen received bytes into buf. Call from a single consumer
// thread (the GUI thread). Returns the number of bytes copied.
size_t flytrap_uart_rx(FlytrapUart* uart, uint8_t* buf, size_t maxlen);

void flytrap_uart_tx(FlytrapUart* uart, const uint8_t* data, size_t len);
