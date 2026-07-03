/*
 * fis_menu.cpp — On-FIS navigation menu implementation
 *
 * FIS screen is 64×88 px (FULLSCREEN).  With COMPACT font each character is
 * approximately 6 px wide and 8 px tall.  We use rows of 10 px so the
 * highlight rect is legible.  A title bar at y=0 + divider at y=9 leaves
 * y=11 for the first item row.
 */

#include "fis_menu.h"
#include "fis_display.h"
#include "config.h"
#include "can_bus.h"
#include "kline.h"
#include "onboot.h"
#include "cards.h"
#include "web_server.h"

#include <TLBFISLib.h>

// ---------------------------------------------------------------------------
// Menu constants
// ---------------------------------------------------------------------------
static const uint8_t kMenuItemHeight = 10;   // px per row (COMPACT font ~8px + 2 gap)
static const uint8_t kMenuStartY     = 12;   // first item y-position (below title+divider)
static const uint8_t kMenuIndentX    = 2;    // text left margin

// ---------------------------------------------------------------------------
// Module list for the K-Line sub-menu
// ---------------------------------------------------------------------------
struct KlineModuleEntry {
    uint8_t     address;
    const char* label;
};

static const KlineModuleEntry kKlineModules[] = {
    { 0x01, "ENGINE" },
    { 0x02, "GEARBOX" },
    { 0x03, "ABS" },
    { 0x22, "AIRBAG" },
    { 0x23, "CLUSTER" },
};
static const uint8_t kKlineModuleCount = (uint8_t)(sizeof(kKlineModules) / sizeof(kKlineModules[0]));

// ---------------------------------------------------------------------------
// Top-level menu items (built dynamically based on enabled features)
// ---------------------------------------------------------------------------
enum TopItem : uint8_t {
    TOP_KLINE = 0,
    TOP_CAN,
    TOP_IGNITRON,
    TOP_HALDEX,
    TOP_SETTINGS,
    TOP_EXIT,
    TOP_COUNT_MAX,
};

// Items in the SETTINGS sub-menu (Y/N toggles + BACK)
enum SettingItem : uint8_t {
    SET_LOGO = 0,
    SET_GREET,
    SET_DATES,
    SET_SHIFT,
    SET_BACK,
    SET_COUNT,
};

// ---------------------------------------------------------------------------
// Menu state
// ---------------------------------------------------------------------------
enum MenuLevel : uint8_t {
    MENU_CLOSED = 0,
    MENU_TOP,
    MENU_KLINE_MODULE,
    MENU_SETTINGS,
};

static MenuLevel s_level      = MENU_CLOSED;
static uint8_t   s_topSel     = 0;   // selected index in top menu
static uint8_t   s_moduleSel  = 0;   // selected index in K-Line module sub-menu
static uint8_t   s_settingsSel = 0;  // selected index in settings sub-menu
static uint8_t   s_topCount   = 0;   // number of visible top-level items

// Remembered "on" values so a Y/N toggle can restore the previous choice
// instead of a hard-coded default. Captured from the live values on menu open.
static uint8_t   s_lastBootScreen   = defaultBootScreen;
static uint16_t  s_lastRpmThreshold = 3000;

// Set when a settings toggle changes a value that must be persisted to NVS.
// The actual NVS write is deferred to task context (menuApplyPendingAction)
// because menuSelect runs in button-callback (ISR) context.
static volatile bool s_pendingSettingsSave = false;

// Pending module reconnect (serviced by klineTask)
static volatile bool    s_klineReconnectPending = false;
static volatile uint8_t s_klineReconnectModule  = 0x01;

// Only re-render when navigation or selection actually changed
static bool     s_menuDirty    = false;
static uint32_t s_lastRenderMs = 0;   // for FIS keepalive

// Lockout: ignore nav/select inputs briefly after menu opens or level changes
// to prevent ghost presses from the same OneButton gesture window.
static uint32_t s_navLockUntil = 0;
static const uint32_t kNavLockMs = 300;

// Pending nav-state changes deferred from ISR to task context
enum MenuPendingAction : uint8_t {
    MENU_ACTION_NONE     = 0,
    MENU_ACTION_HALDEX   = 1,
    MENU_ACTION_CAN      = 2,
    MENU_ACTION_EXIT     = 3,
    MENU_ACTION_KLINE    = 4,
    MENU_ACTION_IGNITRON = 5,
};static volatile MenuPendingAction s_pendingAction = MENU_ACTION_NONE;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build the list of visible top items into a static array and return count.
// Items are only shown if the corresponding feature is enabled.
struct TopEntry { TopItem id; const char* label; };

static uint8_t buildTopItems(TopEntry out[TOP_COUNT_MAX]) {
    uint8_t n = 0;
    out[n++] = { TOP_KLINE,  "K-LINE" };          // always present
    if (hasCAN)     { out[n++] = { TOP_CAN,      "CAN"      }; }
    if (hasCAN)     { out[n++] = { TOP_IGNITRON, "IGNITRON" }; }
    if (hasHaldex)  { out[n++] = { TOP_HALDEX,   "HALDEX"   }; }
    out[n++] = { TOP_SETTINGS, "SETTINGS" };      // always present
    out[n++] = { TOP_EXIT, "EXIT" };
    return n;
}

// Find the module index that matches the current klineDefaultModule.
static uint8_t currentModuleIndex() {
    for (uint8_t i = 0; i < kKlineModuleCount; i++) {
        if (kKlineModules[i].address == klineDefaultModule) return i;
    }
    return 0;
}

// Draw one menu row.  If selected: fill rect with INVERTED draw colour.
static void drawMenuRow(uint8_t y, const char* text, bool selected) {
    if (selected) {
        FIS.drawRect(0, y, 64, kMenuItemHeight, TLBFISLib::FILLED);
        FIS.setDrawColor(TLBFISLib::INVERTED);
        FIS.writeText(kMenuIndentX, y + 1, text);
        FIS.setDrawColor(TLBFISLib::NORMAL);
    } else {
        FIS.writeText(kMenuIndentX, y + 1, text);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void menuOpen() {
    if (s_level != MENU_CLOSED) return;

    // Snap module selection to current default
    s_moduleSel = currentModuleIndex();

    // Capture the current "on" values so the settings Y/N toggles can restore
    // them after an off/on cycle instead of falling back to a hard default.
    if (bootScreenSelection != 0) s_lastBootScreen   = bootScreenSelection;
    if (rpmLogoThreshold != 0)    s_lastRpmThreshold = rpmLogoThreshold;

    // Build top item list to know count, keep top selection in range
    TopEntry items[TOP_COUNT_MAX];
    s_topCount = buildTopItems(items);
    if (s_topSel >= s_topCount) s_topSel = 0;

    s_level = MENU_TOP;
    s_menuDirty = true;
    s_navLockUntil = millis() + kNavLockMs;
}

void menuClose() {
    s_level = MENU_CLOSED;
}

bool isMenuOpen() {
    return s_level != MENU_CLOSED;
}

void menuNavigateUp() {
    if (millis() < s_navLockUntil) return;
    if (s_level == MENU_TOP) {
        if (s_topSel == 0) s_topSel = s_topCount - 1;
        else                s_topSel--;
    } else if (s_level == MENU_KLINE_MODULE) {
        // +1 for the BACK item at the end
        const uint8_t total = kKlineModuleCount + 1;
        if (s_moduleSel == 0) s_moduleSel = total - 1;
        else                   s_moduleSel--;
    } else if (s_level == MENU_SETTINGS) {
        if (s_settingsSel == 0) s_settingsSel = SET_COUNT - 1;
        else                     s_settingsSel--;
    }
    s_menuDirty = true;
}

void menuNavigateDown() {
    if (millis() < s_navLockUntil) return;
    if (s_level == MENU_TOP) {
        s_topSel = (s_topSel + 1) % s_topCount;
    } else if (s_level == MENU_KLINE_MODULE) {
        s_moduleSel = (s_moduleSel + 1) % (kKlineModuleCount + 1);
    } else if (s_level == MENU_SETTINGS) {
        s_settingsSel = (s_settingsSel + 1) % SET_COUNT;
    }
    s_menuDirty = true;
}

void menuSelect() {
    if (millis() < s_navLockUntil) return;
    if (s_level == MENU_TOP) {
        TopEntry items[TOP_COUNT_MAX];
        buildTopItems(items);
        const TopItem chosen = items[s_topSel].id;

        switch (chosen) {
            case TOP_KLINE:
                // Enter K-Line module sub-menu
                s_moduleSel = currentModuleIndex();
                s_level = MENU_KLINE_MODULE;
                s_menuDirty = true;
                s_navLockUntil = millis() + kNavLockMs;
                break;

            case TOP_CAN:
                // Defer to task context to avoid ISR-unsafe NVS call
                s_pendingAction = MENU_ACTION_CAN;
                menuClose();
                break;

            case TOP_IGNITRON:
                // Defer to task context to avoid ISR-unsafe NVS call
                s_pendingAction = MENU_ACTION_IGNITRON;
                menuClose();
                break;

            case TOP_HALDEX:
                // Defer to task context to avoid ISR-unsafe NVS call
                s_pendingAction = MENU_ACTION_HALDEX;
                menuClose();
                break;

            case TOP_SETTINGS:
                // Enter the settings sub-menu (Y/N toggles)
                s_settingsSel = 0;
                s_level = MENU_SETTINGS;
                s_menuDirty = true;
                s_navLockUntil = millis() + kNavLockMs;
                break;

            case TOP_EXIT:
            default:
                // Defer screen clear + disable to task context so SPI is safe
                s_pendingAction = MENU_ACTION_EXIT;
                menuClose();
                break;
        }
    } else if (s_level == MENU_KLINE_MODULE) {
        // Last item is BACK
        if (s_moduleSel == kKlineModuleCount) {
            s_level = MENU_TOP;
            s_menuDirty = true;
            s_navLockUntil = millis() + kNavLockMs;
        } else {
            // User picked a module — defer bootSource + NVS save to task context,
            // then trigger reconnect (also serviced in task context via klineTask).
            const uint8_t newAddr = kKlineModules[s_moduleSel].address;
            klineDefaultModule     = newAddr;
            s_klineReconnectModule = newAddr;
            s_pendingAction        = MENU_ACTION_KLINE;
            menuClose();
        }
    } else if (s_level == MENU_SETTINGS) {
        // Toggle the in-memory value immediately (safe in ISR context) and flag
        // the NVS persist for task context. Boot logo / shift light remember the
        // previous "on" value so an off/on cycle restores the user's choice.
        switch (s_settingsSel) {
            case SET_LOGO:
                if (bootScreenSelection != 0) {
                    s_lastBootScreen    = bootScreenSelection;
                    bootScreenSelection = 0;
                } else {
                    bootScreenSelection = s_lastBootScreen ? s_lastBootScreen : (uint8_t)defaultBootScreen;
                    // Logo and greeting are mutually exclusive.
                    welcomeGreetingEnabled = false;
                }
                s_pendingSettingsSave = true;
                break;
            case SET_GREET:
                welcomeGreetingEnabled = !welcomeGreetingEnabled;
                // Logo and greeting are mutually exclusive: enabling the
                // greeting turns the boot logo off (remembering the choice).
                if (welcomeGreetingEnabled && bootScreenSelection != 0) {
                    s_lastBootScreen    = bootScreenSelection;
                    bootScreenSelection = 0;
                }
                s_pendingSettingsSave  = true;
                break;
            case SET_DATES:
                specialDatesEnabled   = !specialDatesEnabled;
                s_pendingSettingsSave = true;
                break;
            case SET_SHIFT:
                if (rpmLogoThreshold != 0) {
                    s_lastRpmThreshold = rpmLogoThreshold;
                    rpmLogoThreshold   = 0;
                } else {
                    rpmLogoThreshold = s_lastRpmThreshold ? s_lastRpmThreshold : 3000;
                }
                s_pendingSettingsSave = true;
                break;
            case SET_BACK:
            default:
                s_level        = MENU_TOP;
                s_navLockUntil = millis() + kNavLockMs;
                break;
        }
        s_menuDirty = true;
    }
}

void menuRender() {
    // Keepalive: call update() every 5s so the FIS connection doesn't time out.
    // The library docs say update() "must be called while not doing anything / waiting".
    if (!s_menuDirty) {
        if (millis() - s_lastRenderMs >= 5000) {
            FIS.update();
            s_lastRenderMs = millis();
        }
        return;
    }
    s_menuDirty = false;
    s_lastRenderMs = millis();
    FIS.clear();
    FIS.setFont(TLBFISLib::COMPACT);
    FIS.setTextAlignment(TLBFISLib::LEFT);
    FIS.setDrawColor(TLBFISLib::NORMAL);

    if (s_level == MENU_TOP) {
        FIS.writeText(kMenuIndentX, 1, "SOURCE");
        FIS.drawLine(0, 9, 64);

        TopEntry items[TOP_COUNT_MAX];
        const uint8_t count = buildTopItems(items);

        for (uint8_t i = 0; i < count; i++) {
            const uint8_t y = kMenuStartY + i * kMenuItemHeight;
            drawMenuRow(y, items[i].label, i == s_topSel);
        }

    } else if (s_level == MENU_KLINE_MODULE) {
        FIS.writeText(kMenuIndentX, 1, "K-LINE MOD");
        FIS.drawLine(0, 9, 64);

        for (uint8_t i = 0; i < kKlineModuleCount; i++) {
            const uint8_t y = kMenuStartY + i * kMenuItemHeight;
            drawMenuRow(y, kKlineModules[i].label, i == s_moduleSel);
        }
        // BACK item
        const uint8_t backY = kMenuStartY + kKlineModuleCount * kMenuItemHeight;
        drawMenuRow(backY, "BACK", s_moduleSel == kKlineModuleCount);

    } else if (s_level == MENU_SETTINGS) {
        FIS.writeText(kMenuIndentX, 1, "SETTINGS");
        FIS.drawLine(0, 9, 64);

        const struct { const char* label; bool on; } toggles[] = {
            { "LOGO",  bootScreenSelection != 0 },
            { "GREET", welcomeGreetingEnabled  },
            { "DATES", specialDatesEnabled      },
            { "SHIFT", rpmLogoThreshold != 0    },
        };
        char row[16];
        for (uint8_t i = 0; i < 4; i++) {
            const uint8_t y = kMenuStartY + i * kMenuItemHeight;
            snprintf(row, sizeof(row), "%-6s %s", toggles[i].label, toggles[i].on ? "Y" : "N");
            drawMenuRow(y, row, i == s_settingsSel);
        }
        const uint8_t backY = kMenuStartY + 4 * kMenuItemHeight;
        drawMenuRow(backY, "BACK", s_settingsSel == SET_BACK);
    }

    FIS.update();
}

// Apply any pending nav-state change deferred from ISR context.
// Must be called from a task (not ISR) — e.g. fisRenderTask.
void menuApplyPendingAction() {
    // Persist any settings-menu toggle (NVS write is unsafe in ISR context).
    if (s_pendingSettingsSave) {
        s_pendingSettingsSave = false;
        persistSettings();
    }

    const MenuPendingAction action = s_pendingAction;
    if (action == MENU_ACTION_NONE) return;
    s_pendingAction = MENU_ACTION_NONE;

    switch (action) {
        case MENU_ACTION_HALDEX:
            showHaldex = true;
            saveNavState();
            break;
        case MENU_ACTION_CAN:
            bootSource  = BOOT_SOURCE_CAN;
            canViewMode = CAN_VIEW_STANDARD;
            if (hasHaldex) showHaldex = false;
            // If K-line was active, stop it and bring CAN back up —
            // mirrors the fallback path in menuServiceKlineReconnect().
            if (hasK) {
                hasK         = false;
                isConnectedK = false;
            }
            if (!hasCAN) {
                hasCAN = true;
                canInit();   // re-installs TWAI driver; canTask resumes from !hasCAN sleep
            }
            saveNavState();
            break;
        case MENU_ACTION_IGNITRON:
            bootSource  = BOOT_SOURCE_CAN;
            canViewMode = CAN_VIEW_IGNITRON;
            if (hasHaldex) showHaldex = false;
            if (hasK) {
                hasK         = false;
                isConnectedK = false;
            }
            if (!hasCAN) {
                hasCAN = true;
                canInit();
            }
            saveNavState();
            break;
        case MENU_ACTION_EXIT:
            // Disable the FIS through the same path as the stalk-button toggle.
            // fisBeenToggled makes the main loop run fisDisablePrep(), which
            // closes the connection cleanly (FIS.clear + FIS.turnOff), drops the
            // K-line, and returns ENA to its default listen state. Tearing the
            // FIS down inline here (in fisRenderTask) left the connection
            // half-open and could hang on the ENA confirmation.
            fisDisable     = true;
            mimickSet      = true;
            fisBeenToggled = true;
            break;
        case MENU_ACTION_KLINE:
            bootSource = BOOT_SOURCE_KLINE;
            showHaldex = false;
            // Switch runtime bus state: stop CAN, enable K-line.
            // isConnectedK is left as-is so menuServiceKlineReconnect() can
            // properly disconnect before reconnecting (handles module switches).
            hasCAN = false;
            isConnectedCAN = false;
            canDeinit();
            hasK = true;
            saveNavState();
            // klineTask will service the actual connection attempt
            s_klineReconnectPending = true;
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Kline reconnect — called from klineTask
// ---------------------------------------------------------------------------
void menuServiceKlineReconnect() {
    if (!s_klineReconnectPending) return;
    s_klineReconnectPending = false;

    const uint8_t addr = s_klineReconnectModule;

    // Resolve human-readable label for the status screen
    const char* moduleName = "MODULE";
    for (uint8_t i = 0; i < kKlineModuleCount; i++) {
        if (kKlineModules[i].address == addr) {
            moduleName = kKlineModules[i].label;
            break;
        }
    }

    LOGKLN("Menu: reconnecting K-line to %s (0x%02X)", moduleName, addr);

    if (isConnectedK) {
        diag.disconnect();
        isConnectedK = false;
    }

    static const uint8_t kMaxAttempts = 3;
    for (uint8_t attempt = 1; attempt <= kMaxAttempts; attempt++) {
        char attemptLine[20];
        snprintf(attemptLine, sizeof(attemptLine), "ATTEMPT %u/%u", attempt, kMaxAttempts);
        LOGKLN("Menu: K-line %s (0x%02X) %s", moduleName, addr, attemptLine);

        // Populate a status card; set clear flag so fisRenderTask wipes the old content first
        fisStatusClearPending = true;
        beginCard(CARD_SOURCE_KLINE, "CONNECTING");
        setCardLine(0, String(moduleName));
        setCardLine(1, String(attemptLine));
        commitCard();
        fisStatusHoldUntilMs = millis() + 5000;

        const KLineKWP1281Lib::executionStatus status =
            diag.attemptConnect(addr, K_Baud);
        if (status == KLineKWP1281Lib::SUCCESS) {
            isConnectedK = true;
            lastBlock = -1;          // Force fisRenderTask to clear before first K-line render
            fisStatusHoldUntilMs = 0; // Release hold so normal K-line polling resumes
            LOGKLN("Menu: K-line connected to 0x%02X", addr);
            return;
        }
        LOGKLN("Menu: K-line connect to 0x%02X attempt %u failed", addr, attempt);
    }

    // All attempts exhausted — restore CAN so it reappears in the menu
    hasCAN     = true;
    hasK       = false;
    bootSource = BOOT_SOURCE_CAN;
    canInit(); // Re-installs the TWAI driver; canTask (sleeping on !hasCAN) will resume
    LOGKLN("Menu: K-line connect failed — reverted to CAN");

    // Show failure card briefly
    fisStatusClearPending = true;
    beginCard(CARD_SOURCE_KLINE, "K-LINE");
    setCardLine(0, String(moduleName));
    setCardLine(1, "CONNECT FAIL");
    commitCard();
    fisStatusHoldUntilMs = millis() + 3000;
    LOGKLN("Menu: K-line connect to 0x%02X failed after %u attempts", addr, kMaxAttempts);
}
