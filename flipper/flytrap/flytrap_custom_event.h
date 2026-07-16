#pragma once

// Custom ViewDispatcher events. Kept above the main-menu item indices (0..N)
// so the two ranges never collide when routed to a scene's on_event.
typedef enum {
    FlytrapEventSsidDone = 100,
    // Posted by the UART worker thread when bytes are available. Drained + parsed
    // globally (flytrap.c) on the GUI thread, then a refresh is dispatched.
    FlytrapEventRxData = 101,
    // Re-render the active view (dashboard/textview) after parsing.
    FlytrapEventRefreshView = 102,
    // Dashboard button elements.
    FlytrapEventShowCaptures = 103,
    FlytrapEventShowConsole = 104,
    // Capture-detail paging.
    FlytrapEventCapturePrev = 105,
    FlytrapEventCaptureNext = 106,
} FlytrapCustomEvent;
