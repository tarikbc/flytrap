#include "flytrap_uart.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>

// GPIO USART on pins 13/14 — the one the WiFi dev board talks over.
#define UART_CH (FuriHalSerialIdUsart)
#define RX_BUF_SIZE (320)

struct FlytrapUart {
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;
    Expansion* expansion;
    uint8_t rx_buf[RX_BUF_SIZE + 1];
    FlytrapUartRxCallback rx_cb;
    void* rx_ctx;
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

static void flytrap_uart_on_irq_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent ev,
    void* context) {
    FlytrapUart* uart = context;
    if(ev == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(uart->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
    }
}

static int32_t flytrap_uart_worker(void* context) {
    FlytrapUart* uart = context;
    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);
        if(events & WorkerEvtStop) break;
        if(events & WorkerEvtRxDone) {
            size_t len = furi_stream_buffer_receive(uart->rx_stream, uart->rx_buf, RX_BUF_SIZE, 0);
            if(len > 0 && uart->rx_cb) {
                uart->rx_buf[len] = '\0';
                uart->rx_cb(uart->rx_buf, len, uart->rx_ctx);
            }
        }
    }
    furi_stream_buffer_free(uart->rx_stream);
    return 0;
}

FlytrapUart* flytrap_uart_init(uint32_t baudrate) {
    FlytrapUart* uart = malloc(sizeof(FlytrapUart));
    memset(uart, 0, sizeof(FlytrapUart));

    uart->rx_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    uart->rx_thread = furi_thread_alloc();
    furi_thread_set_name(uart->rx_thread, "FlytrapUartRx");
    furi_thread_set_stack_size(uart->rx_thread, 2048);
    furi_thread_set_context(uart->rx_thread, uart);
    furi_thread_set_callback(uart->rx_thread, flytrap_uart_worker);
    furi_thread_start(uart->rx_thread);

    // The expansion-module service owns the GPIO USART by default. It MUST be
    // disabled before acquiring the serial handle (else acquire returns NULL and
    // furi_check aborts), and re-enabled after release.
    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    uart->serial_handle = furi_hal_serial_control_acquire(UART_CH);
    furi_check(uart->serial_handle);
    furi_hal_serial_init(uart->serial_handle, baudrate);
    furi_hal_serial_async_rx_start(uart->serial_handle, flytrap_uart_on_irq_cb, uart, false);

    return uart;
}

void flytrap_uart_free(FlytrapUart* uart) {
    furi_assert(uart);

    furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furi_thread_join(uart->rx_thread);
    furi_thread_free(uart->rx_thread);

    furi_hal_serial_async_rx_stop(uart->serial_handle);
    furi_hal_serial_deinit(uart->serial_handle);
    furi_hal_serial_control_release(uart->serial_handle);

    expansion_enable(uart->expansion);
    furi_record_close(RECORD_EXPANSION);

    free(uart);
}

void flytrap_uart_set_rx_callback(FlytrapUart* uart, FlytrapUartRxCallback cb, void* ctx) {
    uart->rx_cb = cb;
    uart->rx_ctx = ctx;
}

void flytrap_uart_tx(FlytrapUart* uart, const uint8_t* data, size_t len) {
    if(uart->serial_handle) {
        furi_hal_serial_tx(uart->serial_handle, data, len);
    }
}
