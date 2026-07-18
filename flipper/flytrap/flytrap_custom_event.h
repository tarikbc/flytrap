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
    // Dashboard -> live clients list.
    FlytrapEventShowClients = 107,
    // Client-detail paging.
    FlytrapEventClientPrev = 108,
    FlytrapEventClientNext = 109,
    // Start flow, each posted after the prior screen paints so a blocking step
    // never freezes on a blank frame: detect the board, then send the portal.
    FlytrapEventDetectBoard = 110,
    FlytrapEventBeginSend = 111,
    // Flash Firmware: start (after the prompt paints), progress, done.
    FlytrapEventFlashStart = 112,
    FlytrapEventFlashProgress = 113,
    FlytrapEventFlashDone = 114,
    // From the "no board" prompt: install firmware, then continue where headed.
    FlytrapEventInstallFirmware = 115,
    FlytrapEventFlashContinue = 116,
} FlytrapCustomEvent;
