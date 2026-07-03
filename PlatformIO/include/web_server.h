#pragma once

/*
 * web_server.h — WiFi AP and AsyncWebServer interface
 */

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

void setupWiFi();
void setupWebServer();

// Persist the current global settings to NVS (used by the on-FIS settings menu).
void persistSettings();