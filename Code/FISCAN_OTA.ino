void setupWiFi() {
  WiFi.hostname(wifiHostName);

#if detailedDebugWiFi
  DEBUG("Beginning WiFi...");
  DEBUG("Creating Access Point...");
#endif

  //WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(wifiHostName);
  WiFi.setSleep(false);  // for the ESP32: turn off sleeping to increase UI responsivness (at the cost of power use)
}

void setupOTA() {
  updateServer.setup(&server, "", "");

  updateServer.onUpdateBegin = [](const UpdateType type, int &result) {
    //you can force abort the update like this if you need to:
    //result = UpdateResult::UPDATE_ABORT;
    Serial.println("Update started : " + String(type));
  };
  updateServer.onUpdateEnd = [](const UpdateType type, int &result) {
    Serial.println("Update finished : " + String(type) + " result: " + String(result));
  };
  server.begin();
}