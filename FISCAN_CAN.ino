void canInit() {
  chassisCAN.setRX(pinCAN_RX);
  chassisCAN.setTX(pinCAN_TX);
  chassisCAN.begin();
  chassisCAN.setBaudRate(500000);
  chassisCAN.onReceive(onBodyRX);

  // set filters up for focusing on only MOT1 / MOT 2?
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

  switch (frame.id) {
    case MOTOR1_ID:
      // frame[2] (byte 3) > motor speed low byte
      // frame[3] (byte 4) > motor speed high byte
      // conversion: 0.25*HEX
      vehicleRPM = (frame.buf[3] << 8 + frame.buf[2]) * 0.25;
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
    default:
      // do nothing...
      break;
  }
}

void parseCAN(){

}