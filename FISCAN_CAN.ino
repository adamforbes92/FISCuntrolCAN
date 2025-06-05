void canInit() {
  chassisCAN.setRX(pinCAN_RX);
  chassisCAN.setTX(pinCAN_TX);
  chassisCAN.begin();
  chassisCAN.setBaudRate(500000);
  chassisCAN.onReceive(onBodyRX);
}

void onBodyRX(const CAN_message_t& frame) {
#if ChassisCANDebug  // print incoming CAN messages
  // print CAN messages to Serial
  Serial.print("Length Recv: ");
  Serial.print(frame.len);
  Serial.print(" CAN ID: ");
  Serial.print(frame.id, HEX);
  Serial.print(" Buffer: ");
  for (uint8_t i = 0; i < frame.len; i++) {
    Serial.print(frame.buf[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
#endif
  isConnectedCAN = true;
  lastTransmission = millis();

  switch (frame.id) {
    case MOTOR1_ID:
      // frame[2] (byte 3) > motor speed low byte
      // frame[3] (byte 4) > motor speed high byte
      // conversion: 0.25*HEX
      vehicleRPM = ((frame.buf[3] << 8) | frame.buf[2]) * 0.25;
      break;
    case MOTOR2_ID:
      calcSpeed = (frame.buf[3] * 100 * 128) / 10000;
      vehicleSpeed = (byte)(calcSpeed >= 255 ? 255 : calcSpeed);
      break;
    case MOTOR5_ID:
      // frame[1] > free, bit 0
      // frame[1] > vorgl lampeu, bit 1
      // frame[1] > EPC lamp, bit 2
      // frame[1] > ODB2 (EML?) lamp, bit 3
      // frame[1] > cat lamp, bit 4

      // set EML & EPC based on the bit read (LSB, so backwards)
      vehicleEML = bitRead(frame.buf[1], 5);
      vehicleEPC = bitRead(frame.buf[1], 6);
      break;
    case openHaldex_ID:
      //appMessageStatus = frame.buf[0];  //
      //frame.buf[1] = 0x00;                 //
      haldexEngagement = map(frame.buf[2], 128, 198, 0, 100);  //
      lockTarget = frame.buf[3];                               //
      vehicleSpeed = frame.buf[4];                             //
      state.mode_override = frame.buf[5];                      //
      if (!hasOpenHaldex) {
        lastMode = frame.buf[6];  //
      }
      pedValue = frame.buf[7];  //
      hasOpenHaldex = true;
      break;
    default:
      // do nothing...
      break;
  }
}

void parseCAN() {
#if 0  //serialDebug
  Serial.println();
  Serial.print("vehicleRPM: ");
  DEBUG_PRINTLN(vehicleRPM);

  Serial.print("vehicleSpeed: ");
  DEBUG_PRINTLN(vehicleSpeed);

  Serial.print("vehicleEML: ");
  DEBUG_PRINTLN(vehicleEML);

  Serial.print("vehicleEPC: ");
  DEBUG_PRINTLN(vehicleEPC);
#endif

  if (showHaldex) {
    char buf[20];
    char buf1[20];
    char buf2[40];
    char buf3[40];
    char buf4[40];
    char buf5[40];

    switch (lastMode) {
      case 0:
        sprintf(buf, "Mode: Stock");
        break;
      case 1:
        sprintf(buf, "Mode: FWD");
        break;
      case 2:
        sprintf(buf, "Mode: 5050");
        break;
      case 3:
        sprintf(buf, "Mode: 7525");
        break;
      default:
        sprintf(buf, "Error!");
        break;
    }
    if (hasOpenHaldex) {
      sprintf(buf1, "Conn.: Yes");
    } else {
      sprintf(buf1, "Conn.: No");
    }

    sprintf(buf2, "Act. Lock: %d%", haldexEngagement);
    sprintf(buf3, "Req. Lock: %d%", lockTarget);
    sprintf(buf4, "Speed: %d%", vehicleSpeed);
    sprintf(buf5, "Pedal: %d%", pedValue);

    fisLine[0] = String(buf1);
    fisLine[1] = String(buf);
    fisLine[2] = String(buf2);
    fisLine[3] = String(buf3);
    fisLine[4] = String(buf4);
    fisLine[5] = String(buf5);
  } else {
    fisLine[0] = "RPM: " + String(vehicleRPM);
    fisLine[1] = "Speed: " + String(vehicleSpeed);

    if (vehicleEML) {
      fisLine[2] = "EML: On";
    } else {
      fisLine[2] = "EML: Off";
    }
    if (vehicleEPC) {
      fisLine[3] = "EPC: On";
    } else {
      fisLine[3] = "EPC: Off";
    }
  }
}

void broadcastOpenHaldex() {
  switch (lastMode) {
    case 0:
      state.mode = MODE_STOCK;
      break;
    case 1:
      state.mode = MODE_FWD;
      break;
    case 2:
      state.mode = MODE_5050;
      break;
    case 3:
      state.mode = MODE_7525;
      break;
  }

  CAN_message_t broadcastCAN;  //0x7C0
  broadcastCAN.id = fisCuntrol_ID;
  broadcastCAN.len = 8;
  broadcastCAN.buf[0] = lastMode;  //
  broadcastCAN.buf[1] = 0x00;      //
  broadcastCAN.buf[2] = 0x00;      //
  broadcastCAN.buf[3] = 0x00;      //
  broadcastCAN.buf[4] = 0x00;      //
  broadcastCAN.buf[5] = 0x00;      //
  broadcastCAN.buf[6] = 0x00;      //
  broadcastCAN.buf[7] = 0x00;      //

  if (!chassisCAN.write(broadcastCAN)) {              // write CAN frame from the body to the Haldex
    Serial.println(F("Chassis CAN Write TX Fail!"));  // if writing is unsuccessful, there is something wrong with the Haldex(!) Possibly flash red LED?
  }
}