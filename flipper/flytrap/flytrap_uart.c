#include "flytrap_uart.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>

// GPIO USART on pins 13/14 — the one the WiFi dev board talks over.
#define UART_CH (FuriHalSerialIdUsart)
#define RX_STREAM_SIZE (1024)

struct FlytrapUart {
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream; // IRQ producer -> GUI consumer (SPSC)
    FuriHalSerialHandle* serial_handle;
    Expansion* expansion;
    uint32_t baudrate; // normal portal baud, restored by resume
    FlytrapUartNotify notify; // fixed at init
    void* notify_ctx; // fixed at init
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

// Serial RX interrupt: push the byte to the stream and wake the worker.
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

// Worker only bridges the ISR to the consumer thread: it signals via notify()
// (send_custom_event is thread-safe but NOT ISR-safe, so it can't run in the
// IRQ). It does not touch app state or the stream, so there are no data races.
static int32_t flytrap_uart_worker(void* context) {
    FlytrapUart* uart = context;
    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);
        if(events & WorkerEvtStop) break;
        if(events & WorkerEvtRxDone) {
            if(uart->notify) uart->notify(uart->notify_ctx);
        }
    }
    return 0;
}

FlytrapUart* flytrap_uart_init(uint32_t baudrate, FlytrapUartNotify notify, void* notify_ctx) {
    FlytrapUart* uart = malloc(sizeof(FlytrapUart));
    memset(uart, 0, sizeof(FlytrapUart));
    uart->notify = notify;
    uart->notify_ctx = notify_ctx;
    uart->baudrate = baudrate;

    uart->rx_stream = furi_stream_buffer_alloc(RX_STREAM_SIZE, 1);
    uart->rx_thread = furi_thread_alloc();
    furi_thread_set_name(uart->rx_thread, "FlytrapUartRx");
    furi_thread_set_stack_size(uart->rx_thread, 1024);
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

void flytrap_uart_stop_rx(FlytrapUart* uart) {
    furi_assert(uart);
    if(!uart->rx_thread) return;
    // Stop the RX interrupt first, then the worker, so no notify fires after this.
    // The serial handle stays valid for a final tx until flytrap_uart_deinit().
    furi_hal_serial_async_rx_stop(uart->serial_handle);
    furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furi_thread_join(uart->rx_thread);
    furi_thread_free(uart->rx_thread);
    uart->rx_thread = NULL;
}

void flytrap_uart_deinit(FlytrapUart* uart) {
    furi_assert(uart);
    flytrap_uart_stop_rx(uart); // idempotent

    furi_hal_serial_deinit(uart->serial_handle);
    furi_hal_serial_control_release(uart->serial_handle);
    furi_stream_buffer_free(uart->rx_stream);

    expansion_enable(uart->expansion);
    furi_record_close(RECORD_EXPANSION);

    free(uart);
}

size_t flytrap_uart_rx(FlytrapUart* uart, uint8_t* buf, size_t maxlen) {
    return furi_stream_buffer_receive(uart->rx_stream, buf, maxlen, 0);
}

void flytrap_uart_tx(FlytrapUart* uart, const uint8_t* data, size_t len) {
    if(uart->serial_handle) {
        furi_hal_serial_tx(uart->serial_handle, data, len);
    }
}

FuriHalSerialHandle* flytrap_uart_serial(FlytrapUart* uart) {
    return uart->serial_handle;
}

void flytrap_uart_suspend(FlytrapUart* uart) {
    // Stop consuming RX so the flasher owns the line; the worker just parks on its
    // flag-wait (no RxDone events arrive) and needs no teardown.
    furi_hal_serial_async_rx_stop(uart->serial_handle);
}

void flytrap_uart_resume(FlytrapUart* uart) {
    furi_hal_serial_set_br(uart->serial_handle, uart->baudrate);
    furi_hal_serial_async_rx_start(uart->serial_handle, flytrap_uart_on_irq_cb, uart, false);
}
