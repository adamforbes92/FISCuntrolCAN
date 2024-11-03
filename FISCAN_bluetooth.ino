void launchBluetooth() {
  if (hasHaldex) {
#if serialDebug
    Serial.println(F("Beginning Bluetooth Connections..."));
#endif

    // setup a new Bluetooth device with a name and set it to a master role (true)
    if (!SerialBT.begin(deviceName, true)) {
      Serial.println("SerialBT failed!");
    }

    BTScanResults* btDeviceList = SerialBT.getScanResults();  // maybe accessing from different threads!
    if (SerialBT.discoverAsync([](BTAdvertisedDevice* pDevice) {})) {
      delay(btDiscoverTime);
      SerialBT.discoverAsyncStop();

      if (btDeviceList->getCount() > 0) {
        BTAddress addr;
        int channel = 0;
        Serial.println("Found devices:");

        for (int i = 0; i < btDeviceList->getCount(); i++) {
          BTAdvertisedDevice* device = btDeviceList->getDevice(i);

          p = strstr(device->getName().c_str(), "OpenHaldexT4");

          if (p) {
            std::map<int, std::string> channels = SerialBT.getChannels(device->getAddress());
            if (channels.size() > 0) {
              addr = device->getAddress();
              channel = channels.begin()->first;
            }

            if (addr) {
              Serial.printf("connecting to %s - %d\n", addr.toString().c_str(), channel);
              SerialBT.connect(addr, channel, sec_mask, role);
              break;
            }
          }
        }
      } else {
        Serial.println("Didn't find any devices");
      }
    } else {
      Serial.println("Error on discoverAsync f.e. not working after a \"connect\"");
    }
  }
}

void btSendStatus() {
#if serialDebug
  Serial.println(F("Sending status"));
#endif
  if (SerialBT.connected()) {
    if (incomingLen > 0 && !runOnce) {
      btOutgoing[0] = 4;  // APP_MSG_IS_SCREEN override control
      btOutgoing[1] = serialPacketEnd;
      SerialBT.write(btOutgoing, 2);
      runOnce = true;
    }

    btOutgoing[0] = 0;  // APP_MSG_MODE
    btOutgoing[1] = state.mode;
    btOutgoing[2] = pedValue;
    btOutgoing[3] = serialPacketEnd;
    SerialBT.write(btOutgoing, 4);
  }
}

void btReceiveStatus() {
  /*
    incoming packet details (for knowing what each byte is...)
    packet.data[0] = APP_MSG_STATUS;
    packet.data[1] = 0;  // was haldexStatus
    packet.data[2] = haldexEngagement;
    packet.data[3] = lockTarget;
    packet.data[4] = vehicleSpeed;
    packet.data[5] = state.mode_override;
    packet.data[6] = state.mode;
    packet.data[7] = pedValue;
    packet.data[8] = screenSoftwareVersion;
    packet.data[9] = SERIAL_PACKET_END;
    packet.len = 10;
  */

  lastTransmission = millis();

  if (SerialBT.connected()) {
#if serialDebug
    Serial.print("APP_MSG_STATUS: ");
    Serial.println(btIncoming[0]);
    Serial.print("State.mode_override: ");
    Serial.println(btIncoming[5]);
    Serial.print("State.mode: ");
    Serial.print("SERIAL_PACKET_END: ");
    Serial.println(btIncoming[9]);
#endif

    haldexState = btIncoming[1];
    haldexEngagement = btIncoming[2];
    lockTarget = btIncoming[3];
    vehicleSpeed = btIncoming[4];
    //state.mode_override = btIncoming[5];
    //state.mode = btIncoming[6];
    pedValue = btIncoming[7];
    boardSoftwareVersion = btIncoming[8];
  }
}