#pragma once

// Custom ViewDispatcher events. Kept above the main-menu item indices (0..N)
// so the two ranges never collide when routed to a scene's on_event.
typedef enum {
    FlytrapEventSsidDone = 100,
    FlytrapEventRefreshView = 101,
} FlytrapCustomEvent;
