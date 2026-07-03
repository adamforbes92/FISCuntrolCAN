/*
 * web_server.cpp — WiFi soft-AP and web server routes
 */

#include "web_server.h"
#include "config.h"
#include "bootLogos.h"
#include "buttons.h"
#include "cards.h"
#include "onboot.h"
#include "kline.h"
#include "can_bus.h"
#include "power_manager.h"
#include "timekeeper.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Update.h>

AsyncWebServer           server(80);
static Preferences       settingsPrefs;
static const char* customBootLogoUploadPath = "/bootlogo-upload.bmp";
static const char* customRpmLogoUploadPath  = "/rpmlogo-upload.bmp";

static const uint32_t kBusHealthyWindowMs = 2500;
static const uint32_t canHealthyWindowMs  = 2500;

static const char* cardSourceToString(card_source_id source) {
    switch (source) {
        case CARD_SOURCE_KLINE:  return "kline";
        case CARD_SOURCE_CAN:    return "can";
        case CARD_SOURCE_HALDEX: return "haldex";
        default:                 return "none";
    }
}

static const char* buttonNameToString(button_id button) {
    switch (button) {
        case BUTTON_UP:    return "up";
        case BUTTON_DOWN:  return "down";
        case BUTTON_RESET: return "reset";
        default:           return "unknown";
    }
}

static bool buttonFromString(const String& value, button_id* outButton) {
    if (value == "up") {
        *outButton = BUTTON_UP;
        return true;
    }
    if (value == "down") {
        *outButton = BUTTON_DOWN;
        return true;
    }
    if (value == "reset") {
        *outButton = BUTTON_RESET;
        return true;
    }
    return false;
}

static void appendButtonState(JsonDocument& doc, const char* prefix, button_id button) {
    String inputPinKey = String(prefix) + "InputPin";
    String inputStateKey = String(prefix) + "InputPressed";
    String outputPinKey = String(prefix) + "OutputPin";
    String outputStateKey = String(prefix) + "OutputActive";
    String outputLevelKey = String(prefix) + "OutputLevel";

    doc[inputPinKey] = getButtonInputPin(button);
    doc[inputStateKey] = getButtonInputPressed(button);
    doc[outputPinKey] = getButtonOutputPin(button);
    doc[outputStateKey] = getButtonOutputActive(button);
    doc[outputLevelKey] = getButtonOutputPhysicalLevel(button) == LOW ? "LOW" : "HIGH";
}

static const char* modeToString(uint8_t mode) {
    switch (mode) {
        case 0: return "Stock";
        case 1: return "FWD";
        case 2: return "50:50";
        case 3: return "60:40";
        case 4: return "75:25";
        case 5: return "Expert";
        default: return "Unknown";
    }
}

static const char* bootSourceToString(boot_source_id source) {
    switch (source) {
        case BOOT_SOURCE_KLINE: return "kline";
        case BOOT_SOURCE_CAN:   return "can";
        case BOOT_SOURCE_NONE:  return "none";
        default:                return "can";
    }
}

static boot_source_id bootSourceFromString(const String& value) {
    if (value == "kline") {
        return BOOT_SOURCE_KLINE;
    }
    if (value == "none") {
        return BOOT_SOURCE_NONE;
    }
    return BOOT_SOURCE_CAN;
}

static bool isCanHealthy() {
    return isConnectedCAN && ((millis() - lastTransmission) <= canHealthyWindowMs);
}

static bool isKlineHealthy() {
    return isConnectedK && ((millis() - lastKlineTransmission) <= kBusHealthyWindowMs);
}

static void loadPersistedSettings() {
    settingsPrefs.begin("settings", true);
    hasFIS    = settingsPrefs.getBool("hasFIS", hasFIS);
    hasCAN    = settingsPrefs.getBool("hasCAN", hasCAN);
    hasK      = settingsPrefs.getBool("hasK", hasK);
    hasHaldex = settingsPrefs.getBool("hasHaldex", hasHaldex);
    logCatEnabled[LOG_SYS]   = settingsPrefs.getBool("logSys",   logCatEnabled[LOG_SYS]);
    logCatEnabled[LOG_CAN]   = settingsPrefs.getBool("logCan",   logCatEnabled[LOG_CAN]);
    logCatEnabled[LOG_KLINE] = settingsPrefs.getBool("logKln",   logCatEnabled[LOG_KLINE]);
    logCatEnabled[LOG_IO]    = settingsPrefs.getBool("logIo",    logCatEnabled[LOG_IO]);
    logCatEnabled[LOG_WIFI]  = settingsPrefs.getBool("logWifi",  logCatEnabled[LOG_WIFI]);
    oilSensorEnabled   = settingsPrefs.getBool("oilEn", oilSensorEnabled);
    serialMirrorToCard = settingsPrefs.getBool("serialCard", serialMirrorToCard);
    rpmLogoThreshold = settingsPrefs.getUShort("rpmLogoTh", rpmLogoThreshold);
    rpmLogoSelection = settingsPrefs.getUChar("rpmLogoSel", rpmLogoSelection);
    // Button pins are fixed at compile time; no NVS remapping.
    bootScreenSelection = settingsPrefs.getUChar("bootScreen", bootScreenSelection);
    // Migrate the retired "Text Welcome" (1) selection: the greeting now has its
    // own toggle, so 1 is no longer a valid boot-logo value. Snap it to the
    // default logo so a device that had a logo "on" still shows one.
    if (bootScreenSelection == 1) {
        bootScreenSelection = defaultBootScreen;
    }
    bootSource = (boot_source_id)settingsPrefs.getUChar("bootSource", (uint8_t)bootSource);
    extOutputEnabled    = settingsPrefs.getBool("extOutput", extOutputEnabled);
    extOutputMode           = settingsPrefs.getUChar("extOutMode", extOutputMode);
    extOutputRpmThreshold   = settingsPrefs.getUShort("extOutRpm", extOutputRpmThreshold);
    extOutputSpeedThreshold = settingsPrefs.getUChar("extOutSpd", extOutputSpeedThreshold);
    klineDefaultModule  = settingsPrefs.getUChar("klineModule", klineDefaultModule);
    welcomeGreetingEnabled = settingsPrefs.getBool("welGreet", welcomeGreetingEnabled);
    specialDatesEnabled    = settingsPrefs.getBool("specDates", specialDatesEnabled);
    useRTC                 = settingsPrefs.getBool("useRTC", useRTC);
    welcomeGreetName       = settingsPrefs.getString("greetName", welcomeGreetName);
    for (int i = 0; i < 5; i++) {
        char k[8];
        snprintf(k, sizeof(k), "sd%d", i);
        specialDateValue[i] = settingsPrefs.getString(k, specialDateValue[i]);
        snprintf(k, sizeof(k), "st%d", i);
        specialDateText[i]  = settingsPrefs.getString(k, specialDateText[i]);
    }
    settingsPrefs.end();
    // Apply ext output pin state after loading
    digitalWrite(extOutputPin, extOutputEnabled ? HIGH : LOW);
}

static void savePersistedSettings() {
    settingsPrefs.begin("settings", false);
    settingsPrefs.putBool("hasFIS", hasFIS);
    settingsPrefs.putBool("hasCAN", hasCAN);
    settingsPrefs.putBool("hasK", hasK);
    settingsPrefs.putBool("hasHaldex", hasHaldex);
    settingsPrefs.putBool("logSys",   logCatEnabled[LOG_SYS]);
    settingsPrefs.putBool("logCan",   logCatEnabled[LOG_CAN]);
    settingsPrefs.putBool("logKln",   logCatEnabled[LOG_KLINE]);
    settingsPrefs.putBool("logIo",    logCatEnabled[LOG_IO]);
    settingsPrefs.putBool("logWifi",  logCatEnabled[LOG_WIFI]);
    settingsPrefs.putBool("oilEn", oilSensorEnabled);
    settingsPrefs.putBool("serialCard", serialMirrorToCard);
    settingsPrefs.putUShort("rpmLogoTh", rpmLogoThreshold);
    settingsPrefs.putUChar("rpmLogoSel", rpmLogoSelection);
    settingsPrefs.putUChar("bootScreen", bootScreenSelection);
    settingsPrefs.putUChar("bootSource", (uint8_t)bootSource);
    settingsPrefs.putBool("extOutput", extOutputEnabled);
    settingsPrefs.putUChar("extOutMode", extOutputMode);
    settingsPrefs.putUShort("extOutRpm", extOutputRpmThreshold);
    settingsPrefs.putUChar("extOutSpd", extOutputSpeedThreshold);
    settingsPrefs.putUChar("klineModule", klineDefaultModule);
    settingsPrefs.putBool("welGreet", welcomeGreetingEnabled);
    settingsPrefs.putBool("specDates", specialDatesEnabled);
    settingsPrefs.putBool("useRTC", useRTC);
    settingsPrefs.putString("greetName", welcomeGreetName);
    for (int i = 0; i < 5; i++) {
        char k[8];
        snprintf(k, sizeof(k), "sd%d", i);
        settingsPrefs.putString(k, specialDateValue[i]);
        snprintf(k, sizeof(k), "st%d", i);
        settingsPrefs.putString(k, specialDateText[i]);
    }
    settingsPrefs.end();
}

static void registerApiRoutes() {
    auto fisHandler = [](AsyncWebServerRequest* request) {
        JsonDocument doc;

        const bool useSerialLogCard = serialMirrorToCard;
        const char* currentSource = cardSourceToString(getCardSource());
        const char* selectedSource = cardSourceToString(getSelectedCardSource());

        doc["title"]            = webUiTitle;
        doc["welcomeMessage"]   = webUiWelcomeMessage;
        doc["viewSource"]       = currentSource;
        doc["selectedSource"]   = selectedSource;
        doc["apiCardMode"]      = useSerialLogCard ? "serial" : "fis";
        doc["apiCardSource"]    = useSerialLogCard ? "serial" : currentSource;
        doc["viewTitle"]        = useSerialLogCard ? "SYSTEM LOG" : getCardTitle();
        JsonArray lines = doc["lines"].to<JsonArray>();
        for (uint8_t i = 0; i < 8; i++) {
            lines.add(useSerialLogCard ? serialLogLines[i] : fisLine[i]);
        }
        doc["ignition"]         = ignitionState;
        doc["fisBootReady"]     = fisBootReady;
        doc["FIS_Feedback"]     = fisFeedback;
        doc["canConnected"]     = isConnectedCAN;
        doc["klineConnected"]   = isConnectedK;
        doc["chassisCAN"]       = isCanHealthy();
        doc["haldexCAN"]        = hasOpenHaldex;
        doc["klineHealthy"]     = isKlineHealthy();
        doc["freeHeap"]         = ESP.getFreeHeap();
        doc["bootSource"]       = bootSourceToString(bootSource);
        doc["serialMirrorToCard"] = serialMirrorToCard;
        doc["rpmLogoThreshold"] = rpmLogoThreshold;
        doc["customRpmLogoAvailable"] = hasCustomRpmLogo();
        doc["FW_VERSION"]       = "FISCuntrolCAN";
        doc["deviceTime"]       = timeNowIso();
        doc["timeValid"]        = timeIsValid();
        doc["rtcPresent"]       = timeRtcPresent();
        doc["rtcHealthy"]       = timeRtcHealthy();
        doc["rtcTime"]          = timeRtcIso();
        appendButtonState(doc, "up", BUTTON_UP);
        appendButtonState(doc, "down", BUTTON_DOWN);
        appendButtonState(doc, "reset", BUTTON_RESET);

        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload);
    };

    server.on("/api/dashboard", HTTP_GET, fisHandler);
    server.on("/api/fis", HTTP_GET, fisHandler);

    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["hasFIS"]         = hasFIS;
        doc["fisBootReady"]   = fisBootReady;
        doc["FIS_Feedback"]   = fisFeedback;
        doc["hasCAN"]         = hasCAN;
        doc["hasK"]           = hasK;
        doc["hasHaldex"]      = hasHaldex;
        JsonObject logCat = doc["logCat"].to<JsonObject>();
        logCat["sys"]   = logCatEnabled[LOG_SYS];
        logCat["can"]   = logCatEnabled[LOG_CAN];
        logCat["kline"] = logCatEnabled[LOG_KLINE];
        logCat["io"]    = logCatEnabled[LOG_IO];
        logCat["wifi"]  = logCatEnabled[LOG_WIFI];
        doc["oilSensorEnabled"] = oilSensorEnabled;
        doc["serialMirrorToCard"] = serialMirrorToCard;
        doc["rpmLogoThreshold"] = rpmLogoThreshold;
        doc["bootSource"]     = bootSourceToString(bootSource);
        doc["bootScreen"]     = bootScreenSelection;
        doc["customBootLogoAvailable"] = hasCustomBootLogo();
        doc["customRpmLogoAvailable"] = hasCustomRpmLogo();
        doc["rpmLogoSelection"] = rpmLogoSelection;
        doc["upInputPin"]     = getButtonInputPin(BUTTON_UP);
        doc["downInputPin"]   = getButtonInputPin(BUTTON_DOWN);
        doc["resetInputPin"]  = getButtonInputPin(BUTTON_RESET);
        doc["upOutputPin"]    = getButtonOutputPin(BUTTON_UP);
        doc["downOutputPin"]  = getButtonOutputPin(BUTTON_DOWN);
        doc["resetOutputPin"] = getButtonOutputPin(BUTTON_RESET);
        doc["title"]          = webUiTitle;
        doc["welcomeMessage"] = webUiWelcomeMessage;
        doc["klineDefaultModule"] = klineDefaultModule;
        doc["extOutputEnabled"]   = extOutputEnabled;
        doc["extOutputMode"]      = extOutputMode;
        doc["extOutputRpmThreshold"]   = extOutputRpmThreshold;
        doc["extOutputSpeedThreshold"] = extOutputSpeedThreshold;
        doc["extOutputPin"]       = extOutputPin;

        doc["welcomeGreetingEnabled"] = welcomeGreetingEnabled;
        doc["specialDatesEnabled"]    = specialDatesEnabled;
        doc["useRTC"]                 = useRTC;
        doc["welcomeGreetName"]       = welcomeGreetName;
        doc["deviceTime"]             = timeNowIso();
        doc["timeValid"]              = timeIsValid();
        doc["rtcPresent"]             = timeRtcPresent();
        JsonArray sd = doc["specialDates"].to<JsonArray>();
        for (int i = 0; i < 5; i++) {
            JsonObject o = sd.add<JsonObject>();
            o["date"] = specialDateValue[i];
            o["text"] = specialDateText[i];
        }

        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload);
    });

    server.on(
        "/api/settings",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"ok\":true}");
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)index;
            (void)total;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                return;
            }

            const boot_source_id previousBootSource = bootSource;

            if (doc["bootSource"].is<const char*>()) {
                bootSource = bootSourceFromString(String(doc["bootSource"].as<const char*>()));
            }
            if (doc["hasFIS"].is<bool>()) {
                hasFIS = doc["hasFIS"].as<bool>();
            }
            if (doc["hasCAN"].is<bool>()) {
                hasCAN = doc["hasCAN"].as<bool>();
            }
            if (doc["hasK"].is<bool>()) {
                hasK = doc["hasK"].as<bool>();
            }
            if (doc["hasHaldex"].is<bool>()) {
                hasHaldex = doc["hasHaldex"].as<bool>();
            }
            if (doc["logCat"].is<JsonObject>()) {
                JsonObject logCat = doc["logCat"].as<JsonObject>();
                if (logCat["sys"].is<bool>())   logCatEnabled[LOG_SYS]   = logCat["sys"].as<bool>();
                if (logCat["can"].is<bool>())   logCatEnabled[LOG_CAN]   = logCat["can"].as<bool>();
                if (logCat["kline"].is<bool>()) logCatEnabled[LOG_KLINE] = logCat["kline"].as<bool>();
                if (logCat["io"].is<bool>())    logCatEnabled[LOG_IO]    = logCat["io"].as<bool>();
                if (logCat["wifi"].is<bool>())  logCatEnabled[LOG_WIFI]  = logCat["wifi"].as<bool>();
            }
            if (doc["oilSensorEnabled"].is<bool>()) {
                oilSensorEnabled = doc["oilSensorEnabled"].as<bool>();
            }
            if (doc["serialMirrorToCard"].is<bool>()) {
                serialMirrorToCard = doc["serialMirrorToCard"].as<bool>();
            }
            if (doc["rpmLogoThreshold"].is<uint16_t>()) {
                const uint16_t requestedThreshold = doc["rpmLogoThreshold"].as<uint16_t>();
                rpmLogoThreshold = (uint16_t)((requestedThreshold / 5U) * 5U);
            }
            if (doc["bootScreen"].is<uint8_t>()) {
                const uint8_t requestedBootScreen = doc["bootScreen"].as<uint8_t>();
                // 1 = retired "Text Welcome" (greeting has its own toggle now).
                if (requestedBootScreen != 1 && requestedBootScreen <= 5 &&
                    (requestedBootScreen != 5 || hasCustomBootLogo())) {
                    bootScreenSelection = requestedBootScreen;
                }
            }
            if (doc["rpmLogoSelection"].is<uint8_t>()) {
                const uint8_t requestedRpmLogo = doc["rpmLogoSelection"].as<uint8_t>();
                // 1 = Danger preset (always allowed); 2 = Custom (only if uploaded).
                if (requestedRpmLogo == 1 || (requestedRpmLogo == 2 && hasCustomRpmLogo())) {
                    rpmLogoSelection = requestedRpmLogo;
                }
            }
            if (doc["klineDefaultModule"].is<uint8_t>()) {
                klineDefaultModule = doc["klineDefaultModule"].as<uint8_t>();
            }
            if (doc["extOutputEnabled"].is<bool>()) {
                extOutputEnabled = doc["extOutputEnabled"].as<bool>();
                digitalWrite(extOutputPin, extOutputEnabled ? HIGH : LOW);
            }
            if (doc["extOutputMode"].is<uint8_t>()) {
                const uint8_t requestedMode = doc["extOutputMode"].as<uint8_t>();
                if (requestedMode <= 2) {
                    extOutputMode = requestedMode;
                }
            }
            if (doc["extOutputRpmThreshold"].is<uint16_t>()) {
                extOutputRpmThreshold = doc["extOutputRpmThreshold"].as<uint16_t>();
            }
            if (doc["extOutputSpeedThreshold"].is<uint8_t>()) {
                extOutputSpeedThreshold = doc["extOutputSpeedThreshold"].as<uint8_t>();
            }

            const bool previousUseRTC = useRTC;
            if (doc["welcomeGreetingEnabled"].is<bool>()) {
                welcomeGreetingEnabled = doc["welcomeGreetingEnabled"].as<bool>();
            }
            if (doc["specialDatesEnabled"].is<bool>()) {
                specialDatesEnabled = doc["specialDatesEnabled"].as<bool>();
            }
            if (doc["useRTC"].is<bool>()) {
                useRTC = doc["useRTC"].as<bool>();
            }
            if (doc["welcomeGreetName"].is<const char*>()) {
                welcomeGreetName = String(doc["welcomeGreetName"].as<const char*>());
            }
            // Greeting and a boot logo are mutually exclusive: turning the
            // greeting on clears any selected logo.
            if (welcomeGreetingEnabled && bootScreenSelection >= 2) {
                bootScreenSelection = 0;
            }
            if (doc["specialDates"].is<JsonArray>()) {
                JsonArray sd = doc["specialDates"].as<JsonArray>();
                int i = 0;
                for (JsonObject o : sd) {
                    if (i >= 5) break;
                    if (o["date"].is<const char*>()) specialDateValue[i] = String(o["date"].as<const char*>());
                    if (o["text"].is<const char*>()) specialDateText[i]  = String(o["text"].as<const char*>());
                    i++;
                }
            }

            if (!hasK) {
                isConnectedK = false;
            }
            if (!(hasCAN || hasHaldex)) {
                isConnectedCAN = false;
            }

            savePersistedSettings();

            if (previousUseRTC != useRTC) {
                timeInit();   // re-apply RTC setting
            }

            if (ignitionState && !fisDisable && previousBootSource != bootSource) {
                refreshConnections();
            }
        });

    // Browser-supplied wall-clock time (no NTP in AP mode). The page POSTs its
    // local date/time on every load so the device clock stays in sync; the
    // value is also persisted (and pushed to the RTC if enabled).
    server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["deviceTime"] = timeNowIso();
        doc["timeValid"]  = timeIsValid();
        doc["rtcPresent"] = timeRtcPresent();
        doc["useRTC"]     = useRTC;
        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload);
    });

    server.on(
        "/api/time",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"ok\":true}");
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)index;
            (void)total;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                return;
            }
            int year   = doc["year"]   | 0;
            int month  = doc["month"]  | 0;
            int day    = doc["day"]    | 0;
            int hour   = doc["hour"]   | 0;
            int minute = doc["minute"] | 0;
            int second = doc["second"] | 0;
            if (year >= 2020 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
                timeSetLocal(year, month, day, hour, minute, second);
            }
        });

    server.on(
        "/api/buttons/press",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"ok\":true}");
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            (void)request;
            (void)index;
            (void)total;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err || !doc["button"].is<const char*>()) {
                return;
            }

            button_id button;
            if (buttonFromString(String(doc["button"].as<const char*>()), &button)) {
                simulateButtonPress(button);
            }
        });

    server.on("/api/ignition/restart", HTTP_POST, [](AsyncWebServerRequest* request) {
        requestRestartIgnitionSequence();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    server.on(
        "/api/ota",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            bool success = !Update.hasError();
            request->send(success ? 200 : 500,
                          "application/json",
                          success ? "{\"success\":true}" : "{\"success\":false}");

            if (success) {
                xTaskCreate(
                    [](void*) {
                        vTaskDelay(pdMS_TO_TICKS(1500));
                        ESP.restart();
                        vTaskDelete(nullptr);
                    },
                    "ota_restart",
                    2048,
                    nullptr,
                    1,
                    nullptr);
            }
        },
        [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            (void)request;
            (void)filename;

            if (index == 0) {
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }

            if (!Update.hasError()) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                }
            }

            if (final) {
                if (!Update.end(true)) {
                    Update.printError(Serial);
                }
            }
        });

    server.on(
        "/api/boot-logo",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            size_t uploadedSize = 0;
            if (LittleFS.exists(customBootLogoUploadPath)) {
                File uploadedFile = LittleFS.open(customBootLogoUploadPath, "r");
                if (uploadedFile) {
                    uploadedSize = uploadedFile.size();
                    uploadedFile.close();
                }
            }

            const bool success = LittleFS.exists(customBootLogoUploadPath) && saveCustomBootLogoFromBmp(customBootLogoUploadPath);
            if (LittleFS.exists(customBootLogoUploadPath)) {
                LittleFS.remove(customBootLogoUploadPath);
            }

            if (success) {
                bootScreenSelection = 5;
                savePersistedSettings();
            }

            JsonDocument doc;
            doc["success"] = success;
            doc["customBootLogoAvailable"] = hasCustomBootLogo();
            doc["uploadedSize"] = uploadedSize;

            String payload;
            serializeJson(doc, payload);
            if (success) {
                request->send(200, "application/json", payload);
            } else {
                String message = "Invalid boot logo size: received ";
                message += uploadedSize;
                message += " bytes, expected 704 bytes.";
                request->send(400, "text/plain", message);
            }
        },
        [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            (void)filename;

            if (index == 0) {
                if (LittleFS.exists(customBootLogoUploadPath)) {
                    LittleFS.remove(customBootLogoUploadPath);
                }
                request->_tempFile = LittleFS.open(customBootLogoUploadPath, "w");
            }

            if (request->_tempFile) {
                request->_tempFile.write(data, len);
            }

            if (final && request->_tempFile) {
                request->_tempFile.close();
            }
        });

    server.on(
        "/api/rpm-logo",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            size_t uploadedSize = 0;
            if (LittleFS.exists(customRpmLogoUploadPath)) {
                File uploadedFile = LittleFS.open(customRpmLogoUploadPath, "r");
                if (uploadedFile) {
                    uploadedSize = uploadedFile.size();
                    uploadedFile.close();
                }
            }

            const bool success = LittleFS.exists(customRpmLogoUploadPath) && saveCustomRpmLogoFromBmp(customRpmLogoUploadPath);
            if (LittleFS.exists(customRpmLogoUploadPath)) {
                LittleFS.remove(customRpmLogoUploadPath);
            }

            if (success) {
                rpmLogoSelection = 2;  // auto-select the freshly uploaded custom image
                savePersistedSettings();
            }

            JsonDocument doc;
            doc["success"] = success;
            doc["customRpmLogoAvailable"] = hasCustomRpmLogo();
            doc["uploadedSize"] = uploadedSize;

            String payload;
            serializeJson(doc, payload);
            if (success) {
                request->send(200, "application/json", payload);
            } else {
                String message = "Invalid RPM logo size: received ";
                message += uploadedSize;
                message += " bytes, expected 704 bytes.";
                request->send(400, "text/plain", message);
            }
        },
        [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            (void)filename;

            if (index == 0) {
                if (LittleFS.exists(customRpmLogoUploadPath)) {
                    LittleFS.remove(customRpmLogoUploadPath);
                }
                request->_tempFile = LittleFS.open(customRpmLogoUploadPath, "w");
            }

            if (request->_tempFile) {
                request->_tempFile.write(data, len);
            }

            if (final && request->_tempFile) {
                request->_tempFile.close();
            }
        });

    server.on("/api/test-shift-logo", HTTP_POST, [](AsyncWebServerRequest* request) {
        // Force the selected shift indicator onto the FIS for 3 seconds; the
        // render task restores the previous display automatically afterwards.
        rpmLogoTestUntilMs = millis() + 3000;
        request->send(200, "application/json", "{\"success\":true}");
    });

    // -----------------------------------------------------------------------
    // K-Line API routes
    // -----------------------------------------------------------------------
    server.on("/api/kline/faults", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!hasK) {
            request->send(400, "application/json", "{\"error\":\"K-line disabled\"}");
            return;
        }
        if (!isConnectedK) {
            request->send(503, "application/json", "{\"error\":\"K-line not connected\"}");
            return;
        }

        static KlineFaultEntry entries[32];
        const int count = readKlineFaults(entries, 32);

        JsonDocument doc;
        if (count < 0) {
            doc["error"] = "Failed to read faults";
            String payload;
            serializeJson(doc, payload);
            request->send(500, "application/json", payload);
            return;
        }

        doc["count"] = count;
        JsonArray faults = doc["faults"].to<JsonArray>();
        for (int i = 0; i < count; i++) {
            JsonObject f = faults.add<JsonObject>();
            f["code"]          = entries[i].code;
            f["description"]   = entries[i].description;
            f["elaboration"]   = entries[i].elaboration;
            f["intermittent"]  = entries[i].isIntermittent;
        }
        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload);
    });

    server.on("/api/kline/faults/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!hasK) {
            request->send(400, "application/json", "{\"error\":\"K-line disabled\"}");
            return;
        }
        if (!isConnectedK) {
            request->send(503, "application/json", "{\"error\":\"K-line not connected\"}");
            return;
        }
        const bool ok = clearKlineFaults();
        JsonDocument doc;
        doc["ok"] = ok;
        String payload;
        serializeJson(doc, payload);
        request->send(ok ? 200 : 500, "application/json", payload);
    });

    // -----------------------------------------------------------------------
    // CAN API routes
    // -----------------------------------------------------------------------
    server.on("/api/can/cards", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["currentIndex"] = getCurrentCanCardIndex();
        doc["connected"]    = isConnectedCAN;
        JsonArray cards = doc["cards"].to<JsonArray>();
        for (uint8_t i = 0; i < getCanCardCount(); i++) {
            cards.add(getCanCardName(i));
        }
        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload);
    });

    server.on(
        "/api/can/cards/select",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"ok\":true}");
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            (void)request; (void)index; (void)total;
            JsonDocument doc;
            if (deserializeJson(doc, data, len) == DeserializationError::Ok && doc["index"].is<uint8_t>()) {
                setCurrentCanCardIndex(doc["index"].as<uint8_t>());
            }
        });

    // -----------------------------------------------------------------------
    // External Output API route
    // -----------------------------------------------------------------------
    server.on("/api/extoutput", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["enabled"] = extOutputEnabled;
        doc["pin"]     = extOutputPin;
        String payload;
        serializeJson(doc, payload);
        request->send(200, "application/json", payload);
    });

    server.on(
        "/api/extoutput",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"ok\":true}");
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            (void)request; (void)index; (void)total;
            JsonDocument doc;
            if (deserializeJson(doc, data, len) == DeserializationError::Ok && doc["enabled"].is<bool>()) {
                extOutputEnabled = doc["enabled"].as<bool>();
                digitalWrite(extOutputPin, extOutputEnabled ? HIGH : LOW);
                savePersistedSettings();
            }
        });
}

void setupWiFi() {
    WiFi.hostname(wifiHostName);

    LOGWIFI("Starting WiFi AP...");

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(
        IPAddress(192, 168, 1, 1),
        IPAddress(192, 168, 1, 1),
        IPAddress(255, 255, 255, 0));
    WiFi.softAP(wifiHostName);
    WiFi.setSleep(false);

    LOGWIFI("WiFi AP started: %s  IP: %s", wifiHostName, WiFi.softAPIP().toString().c_str());
}

void setupWebServer() {
    loadPersistedSettings();

    if (!LittleFS.begin(true)) {
        LOGWIFI("LittleFS mount failed - UI assets unavailable");
    } else {
        loadCustomBootLogo();
        loadCustomRpmLogo();
        if (bootScreenSelection == 5 && !hasCustomBootLogo()) {
            bootScreenSelection = defaultBootScreen;
            savePersistedSettings();
        }
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    }

    registerApiRoutes();
    server.begin();
}

// Public wrapper so other modules (e.g. the on-FIS settings menu) can persist
// the current global settings to NVS without duplicating the key list.
void persistSettings() {
    savePersistedSettings();
}

// ----------------------------------------------------------------------------
// power_manager integration (universal reduced-power module)
// ----------------------------------------------------------------------------
// These override the weak hooks in power_manager.cpp. The device stays fully
// awake while ANY client is associated to the AP. Once the last client leaves,
// the manager's idle timer runs, then turns the radio off and drops the CPU
// clock. A power-cycle (ignition off/on) brings WiFi back automatically.

bool powerIsBusy()
{
    return WiFi.softAPgetStationNum() > 0;
}

// ACTIVE -> REDUCED: close the web server cleanly before the radio drops.
void powerOnEnterReduced()
{
    server.end();
}

// REDUCED -> ACTIVE: bring the AP and web server back. Routes are already
// registered (no need to re-run setupWebServer()), so we only restart the
// radio and the listener.
void powerOnExitReduced()
{
    setupWiFi();
    server.begin();
}