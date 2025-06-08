uint8_t measurement_buffer[80];
uint8_t measurement_body_buffer[4];

void showMeasurements(uint8_t group) {
  // This will contain the amount of measurements in the current group, after calling the readGroup() function.
  uint8_t amount_of_measurements = 0;

  // This flag keeps track if a [Header] was received for the current group, meaning it's of the "header+body" type.
  bool received_group_header = false;

  // In case of the "header+body" groups, the [Body] response is the one that actually contains live data.
  // In theory, the module should report the same number of measurements in the [Header] and in the [Body].
  // Still, the number of measurements received in the [Header] will be stored separately, for correctness.
  uint8_t amount_of_measurements_in_header = 0;

  // For modules which report measuring groups in the "header+body" mode, it is important to do update() when requesting a new group, so the module sends the [Header].
  diag.update();

  // The requested group will be read 5 times, if it exists and no error occurs.
  // If a "header+body" group is encountered, we will do one more step, so that the actual data requests are 5.
  for (uint8_t attempt = 1; attempt <= 1; attempt++) {
    /*
      The readGroup() function can return:
        KLineKWP1281Lib::FAIL                - the requested group does not exist
        KLineKWP1281Lib::ERROR               - communication error
        KLineKWP1281Lib::SUCCESS             - received measurements
        KLineKWP1281Lib::GROUP_BASIC_SETTING - received a [Basic settings] measurement; the buffer contains 10 raw values
        KLineKWP1281Lib::GROUP_HEADER        - received the [Header] for a "header+body" group; need to read again to get the [Body]
        KLineKWP1281Lib::GROUP_BODY          - received the [Body] for a "header+body" group
    */

    // Read the requested group and store the returned value.
    KLineKWP1281Lib::executionStatus readGroup_status;
    // If the group is not of "header+body" type, or if it is and this is the first request, we don't have a [Header] (yet), so `received_group_header=false`.
    // The response to this request will be stored in the larger array.
    // If it is in fact of "header+body" type, the [Header] will be stored in this array.
    if (!received_group_header) {
      readGroup_status = diag.readGroup(amount_of_measurements, group, measurement_buffer, sizeof(measurement_buffer));
    }
    // If the group is of "header+body" type, and this is not the first request, it means we have a header, so `received_group_header=true`.
    // The response to this request will be stored in the smaller array, because it should be the [Body].
    else {
      readGroup_status = diag.readGroup(amount_of_measurements, group, measurement_body_buffer, sizeof(measurement_body_buffer));
    }

    // Check the returned value.
    switch (readGroup_status) {
      case KLineKWP1281Lib::ERROR:
        {
          DEBUG_PRINTLN("Error reading group!");
        }
        // There is no reason to continue, exit the function.
        return;

      case KLineKWP1281Lib::FAIL:
        {
          DEBUG_PRINT("Group ");
          DEBUG_PRINT(group);
          DEBUG_PRINTLN(" does not exist!");
        }
        // There is no reason to continue, exit the function.
        return;

      case KLineKWP1281Lib::GROUP_BASIC_SETTINGS:
        {
          // If we have a [Header], it means this group sends responses of "header+body" type.
          // So, at this point, it doesn't make sense to receive something other than a [Body].
          if (received_group_header) {
            DEBUG_PRINTLN("Error reading body! (got basic settings)");
            return;
          }

          // We have 10 raw values in the `measurement_buffer` array.
          DEBUG_PRINT("Basic settings in group ");
          DEBUG_PRINT(group);
          DEBUG_PRINT(": ");
          for (uint8_t i = 0; i < 10; i++) {
            DEBUG_PRINT(measurement_buffer[i]);
            DEBUG_PRINT(" ");
          }
          DEBUG_PRINTLN("");
        }
        // We have nothing else to display (yet); skip this step of the loop.
        continue;

      case KLineKWP1281Lib::GROUP_HEADER:
        {
          // If we have a [Header], it means this group sends responses of "header+body" type.
          // So, at this point, it doesn't make sense to receive something other than a [Body].
          if (received_group_header) {
            DEBUG_PRINTLN("Error reading body! (got header)");
            return;
          }

          // Set the flag to indicate that a header was received, making this a "header+body" group response.
          received_group_header = true;

          // Store the number of measurements received in the header.
          amount_of_measurements_in_header = amount_of_measurements;

          // Add an extra step to the loop, because this one shouldn't count, as it doesn't contain live data.
          attempt--;
        }
        // We have nothing to display yet, the next readGroup() will get the actual data; skip this step of the loop.
        continue;

      case KLineKWP1281Lib::GROUP_BODY:
        {
          // If we don't have a [Header], it doesn't make sense to receive a [Body].
          if (!received_group_header) {
            DEBUG_PRINTLN("Error reading header! (got body)");
            return;
          }
        }
        // If we have the [Header], now we also have the [Body]; execute the code after the switch().
        break;

      // Execute the code after the switch().
      case KLineKWP1281Lib::SUCCESS:
        break;
    }

    // If the group was read successfully, display its measurements.
    DEBUG_PRINT("Group ");
    DEBUG_PRINT(group);
    DEBUG_PRINT(':');

    // Display each measurement.
    for (uint8_t i = 0; i < amount_of_measurements; i++) {
      // Format the values with a leading tab.
      DEBUG_PRINT("    ");

      /*
        The getMeasurementType() function can return:
           KLineKWP1281Lib::UNKNOWN - index out of range (the measurement doesn't exist in the group) or the formula is invalid/not-applicable
           KLineKWP1281Lib::VALUE   - regular measurement, with a value and units
           KLineKWP1281Lib::TEXT    - text measurement
      */

      // Get the current measurement's type.
      KLineKWP1281Lib::measurementType measurement_type;
      if (!received_group_header) {
        measurement_type = KLineKWP1281Lib::getMeasurementType(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer));
      }
      // For "header+body" measurements, you need to use this other function that specifically parses headers instead of regular responses.
      else {
        measurement_type = KLineKWP1281Lib::getMeasurementTypeFromHeader(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer));
      }

      // Check the returned value.
      switch (measurement_type) {
        // "Value and units" type
        case KLineKWP1281Lib::VALUE:
          {
            // The measurement's units will be copied into this array, so they can be displayed.
            char units_string[16];

            // This variable will contain the calculated value of the measurement.
            double value;

            // This variable will contain the recommended amount of decimal places for the measurement.
            uint8_t decimals;

            // Regular mode:
            if (!received_group_header) {
              // Calculate the value.
              value = KLineKWP1281Lib::getMeasurementValue(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer));

              // Get the units.
              // It's not necessary to use the returned value of this function (character pointer), the units will appear in the given character array.
              KLineKWP1281Lib::getMeasurementUnits(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer), units_string, sizeof(units_string));

              // Get the recommended amount of decimal places.
              decimals = KLineKWP1281Lib::getMeasurementDecimals(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer));
            }
            // "Header+body" mode:
            else {
              // Calculate the value; both the header and body are needed.
              value = KLineKWP1281Lib::getMeasurementValueFromHeaderBody(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer), amount_of_measurements, measurement_body_buffer, sizeof(measurement_body_buffer));

              // Get the units; both the header and body are needed.
              // It's not necessary to use the returned value of this function (character pointer), the units will appear in the given character array.
              KLineKWP1281Lib::getMeasurementUnitsFromHeaderBody(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer), amount_of_measurements, measurement_body_buffer, sizeof(measurement_body_buffer), units_string, sizeof(units_string));

              // Get the recommended amount of decimal places; only the header is needed.
              decimals = KLineKWP1281Lib::getMeasurementDecimalsFromHeader(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer));
            }
            //DEBUG_PRINT(value, decimals);
            DEBUG_PRINT(' ');
            DEBUG_PRINTLN(units_string);

            fisLine[i] = String(value) + " " + String(units_string);
          }
          break;

        // "Text" type
        case KLineKWP1281Lib::TEXT:
          {
            // The measurement's text will be copied into this array, so it can be displayed.
            char text_string[16];

            if (!received_group_header) {
              // Get the text.
              // It's not necessary to use the returned value of this function (character pointer), the text will appear in the given character array.
              KLineKWP1281Lib::getMeasurementText(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer), text_string, sizeof(text_string));
            } else {
              // Get the text; both the header and body are needed.
              // It's not necessary to use the returned value of this function (character pointer), the text will appear in the given character array.
              KLineKWP1281Lib::getMeasurementTextFromHeaderBody(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer), amount_of_measurements, measurement_body_buffer, sizeof(measurement_body_buffer), text_string, sizeof(text_string));
            }

            DEBUG_PRINTLN(text_string);
            fisLine[i] = String(text_string);
          }
          break;

        // Invalid measurement index
        case KLineKWP1281Lib::UNKNOWN:
          DEBUG_PRINTLN("N/A");
          fisLine[i] = "N/A";
          break;
      }
    }
    DEBUG_PRINTLN("");
  }
}
