#pragma once

/*
 * fis_menu.h — On-FIS navigation menu
 *
 * Long-press Reset opens the menu.
 * Up / Down navigate items.
 * Single-press Reset selects the highlighted item.
 * Long-press Reset exits the menu without changing anything.
 *
 * The menu renders directly to the FIS using TLBFISLib's INVERTED draw colour
 * for the highlighted row (filled rect + inverted text).
 */

#include <Arduino.h>

// Open / close
void menuOpen();
void menuClose();
bool isMenuOpen();

// Navigation (called from button handlers)
void menuNavigateUp();
void menuNavigateDown();
void menuSelect();      // single-press Reset

// Render the current menu frame to the FIS (no FIS.update() — caller does that)
void menuRender();

// Called by fisRenderTask to apply nav state changes safely outside ISR context
void menuApplyPendingAction();

// Called by klineTask to service a pending module reconnect request
void menuServiceKlineReconnect();
