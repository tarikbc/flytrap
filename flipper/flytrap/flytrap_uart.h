#pragma once

#include <furi.h>

typedef struct FlytrapUart FlytrapUart;

// Called from the UART worker thread with a chunk of received bytes.
// The buffer is NUL-terminated (buf[len] == 0) for convenience.
typedef void (*FlytrapUartRxCallback)(uint8_t* buf, size_t len, void* ctx);

FlytrapUart* flytrap_uart_init(uint32_t baudrate);
void flytrap_uart_free(FlytrapUart* uart);
void flytrap_uart_set_rx_callback(FlytrapUart* uart, FlytrapUartRxCallback cb, void* ctx);
void flytrap_uart_tx(FlytrapUart* uart, const uint8_t* data, size_t len);
