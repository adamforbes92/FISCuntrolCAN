void showMeasurements(uint8_t block) {
  // This will contain the amount of measurements in the current block, after calling the readGroup() function.
  uint8_t amount_of_measurements = 0;

  // When requesting a new group,  modules which report measuring blocks in the "header+body" mode will send a header.
  // This parameter must be set to true and given to readGroup when the header for the current group is contained in the buffer.
  bool have_header = false;

  // For modules which report measuring blocks in the "header+body" mode, it is important to do update() when requesting
  // a new group, so the module sends the group's header.
  diag.update();

  // The requested group will be read 5 times, if it exists and no error occurs.
  // If we get a "header+body" measurement, we will do one more step so the actual data requests are 5.
  for (uint8_t attempt = 1; attempt <= 1; attempt++) {
    /*
      The readGroup() function can return:
        KLineKWP1281Lib::SUCCESS             - received measurements
        KLineKWP1281Lib::FAIL                - the requested block does not exist
        KLineKWP1281Lib::ERROR               - communication error
        KLineKWP1281Lib::GROUP_HEADER        - received header for a "header+body" measurement; need to read again
        KLineKWP1281Lib::GROUP_BASIC_SETTING - received a "basic settings" measurement; the buffer contains 10 raw values
    */

    // Read the requested group and store the return value.
    // The last parameter start as false, and will need to be set to true if the header of a "header+body" measurement is received.
    KLineKWP1281Lib::executionStatus readGroup_status = diag.readGroup(amount_of_measurements, block, measurements, sizeof(measurements), have_header);

    // Check the return value.
    switch (readGroup_status) {
      case KLineKWP1281Lib::ERROR:
        {
#if serialDebug
          Serial.println("Error reading measurements!");
#endif
        }
        // There is no reason to continue, exit the function.
        return;

      case KLineKWP1281Lib::FAIL:
        {
#if serialDebug
          Serial.print("Block ");
          Serial.print(block);
          Serial.println(" does not exist!");
#endif
        }
        // There is no reason to continue, exit the function.
        return;

      case KLineKWP1281Lib::GROUP_HEADER:
        {
          // It doesn't make sense to receive a header when we already have it (we are expecting a body).
          if (have_header) {
#if serialDebug
            Serial.println("Error reading group body!");
#endif

            // There is no reason to continue, exit the function.
            return;
          }

          // We now have the header of a "header+body" measurement, it's stored in the `measurements` array.
          have_header = true;

          // Add an extra step to the loop.
          attempt--;
        }
        // We have nothing to display yet, the next readGroup() calls will get the actual data; skip this step of the loop.
        continue;

      case KLineKWP1281Lib::GROUP_BASIC_SETTING:
        {
#if serialDebug
          Serial.print("Basic settings: ");
#endif

          // We have 10 raw values in the `measurements` array.
          for (uint8_t i = 0; i < 10; i++) {
            Serial.print(measurements[i]);
            Serial.print(" ");
          }
          Serial.println();
        }
        // We have nothing else to display yet; skip this step of the loop.
        continue;

      // Execute the code after the switch().
      case KLineKWP1281Lib::SUCCESS:
        break;
    }

// If the block was read successfully, display its measurements.
#if serialDebug
    Serial.print("Block ");
    Serial.print(block);
    Serial.println(':');
#endif

    // Display each measurement.
    for (uint8_t i = 0; i < 4; i++) {
// Format the values with a leading tab.
#if serialDebug
      Serial.print('\t');
#endif

      // You can retrieve the "formula" byte for a measurement, to avoid giving all these parameters to the other functions.
      uint8_t formula = KLineKWP1281Lib::getFormula(i, amount_of_measurements, measurements, sizeof(measurements));

      /*
        The getMeasurementType() function can return:
           KLineKWP1281Lib::UNKNOWN - index out of range (measurement doesn't exist in block)
           KLineKWP1281Lib::VALUE   - regular measurement, with a value and units
           KLineKWP1281Lib::TEXT    - text measurement
      */

      // Get the current measurement's type.
      KLineKWP1281Lib::measurementType measurement_type = KLineKWP1281Lib::getMeasurementType(formula);
      // If you don't want to extract the "formula" byte as shown above with the getFormula() function, getMeasurementType() can also take the same parameters
      // like the other functions (index, amount, buffer, buffer_size).

      // Check the return value.
      switch (measurement_type) {
        // "Value and units" type
        case KLineKWP1281Lib::VALUE:
          {
            // You can retrieve the other significant bytes for a measurement, to avoid giving all these parameters to the other functions.
            uint8_t NWb = KLineKWP1281Lib::getNWb(i, amount_of_measurements, measurements, sizeof(measurements));
            uint8_t MWb = KLineKWP1281Lib::getMWb(i, amount_of_measurements, measurements, sizeof(measurements));

            // This will hold the measurement's units.
            char units_string[16];

            // Get the current measurement's value.
            double value = KLineKWP1281Lib::getMeasurementValue(formula, NWb, MWb);
            // If you don't want to extract the "formula", "NWb" and "MWb" bytes as shown above with the getFormula(), getNWb() and getMWb() functions,
            // getMeasurementValue() and getMeasurementUnits() can also take the same parameters like the other functions (index, amount, buffer, buffer_size).

            // Get the current measurement's units.
            KLineKWP1281Lib::getMeasurementUnits(formula, NWb, MWb, units_string, sizeof(units_string));
            //The getMeasurementUnits() function returns the same string it's given, units_string in this case.

            //Determine how many decimal places are best suited to this measurement.
            uint8_t decimals = KLineKWP1281Lib::getMeasurementDecimals(formula);
// getMeasurementDecimals() only needs to know the "formula", but you can also give it all parameters as with all other functions.

// Display the calculated value, with the recommended amount of decimals.
#if serialDebug
            Serial.print(value, decimals);
            Serial.print(' ');
            Serial.println(units_string);
#endif

            fisLine[i] = String(value) + " " + String(units_string);
          }
          break;

        // "Text" type
        case KLineKWP1281Lib::TEXT:
          {
            // This will hold the measurement's text.
            char text_string[16];

            // The only important values are stored in the text string.
            // The getMeasurementUnits() function needs more data than just those 3 bytes.
            // It's easier to just give it all parameters, like done here.
            KLineKWP1281Lib::getMeasurementText(i, amount_of_measurements, measurements, sizeof(measurements), text_string, sizeof(text_string));

// Display the text.
#if serialDebug
            Serial.println(text_string);
#endif
            fisLine[i] = String(text_string);
          }
          break;

        // Invalid measurement index
        case KLineKWP1281Lib::UNKNOWN:
#if serialDebug
          Serial.println("N/A");
#endif
          fisLine[i] = "N/A";
          break;
      }
    }

// Leave an empty line.
#if serialDebug
    Serial.println();
#endif
  }
}
