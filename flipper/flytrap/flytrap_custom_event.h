#pragma once

// Custom ViewDispatcher events. Kept above the main-menu item indices (0..N)
// so the two ranges never collide when routed to a scene's on_event.
typedef enum {
    FlytrapEventSsidDone = 100,
    // Posted by the UART worker thread when bytes are available. The Live scene
    // drains and parses them on the GUI thread (so all app-state mutation is
    // single-threaded).
    FlytrapEventRxData = 101,
} FlytrapCustomEvent;
