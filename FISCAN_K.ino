void showMeasurements(uint8_t block)
{
  // This will contain the amount of measurements in the current block, after calling the readGroup() function.
  uint8_t amount_of_measurements = 0;
  
  /*
    The readGroup() function can return:
      *KLineKWP1281Lib::SUCCESS - received measurements
      *KLineKWP1281Lib::FAIL    - the requested block does not exist
      *KLineKWP1281Lib::ERROR   - communication error
  */
  
  // Read the requested group and store the return value.
  KLineKWP1281Lib::executionStatus readGroup_status = diag.readGroup(amount_of_measurements, block, measurements, sizeof(measurements));
  
  // Check the return value.
  switch (readGroup_status)
  {
    case KLineKWP1281Lib::ERROR:
      Serial.println("Error reading measurements!");
      return;
    
    case KLineKWP1281Lib::FAIL:
      Serial.print("Block ");
      Serial.print(block);
      Serial.println(" does not exist!");
      return;
    
    // Execute the code after the switch().
    case KLineKWP1281Lib::SUCCESS:
      break;
  }
  
  // If the block was read successfully, display its measurements.
  Serial.print("Block ");
  Serial.print(block);
  Serial.println(':');
    
  // Display each measurement.
  for (uint8_t i = 0; i < 4; i++)
  {
    // Format the values with a leading tab.
    Serial.print('\t');
    
    /*
      The getMeasurementType() function can return:
        *KLineKWP1281Lib::UNKNOWN - index out of range (measurement doesn't exist in block)
        *KLineKWP1281Lib::VALUE   - regular measurement, with a value and units
        *KLineKWP1281Lib::TEXT    - text measurement
    */
    
    //Get the current measurement's type and check the return value.
    switch (KLineKWP1281Lib::getMeasurementType(i, amount_of_measurements, measurements, sizeof(measurements)))
    {
      // "Value and units" type
      case KLineKWP1281Lib::VALUE:
      {
        // This will hold the measurement's units.
        char units_string[16];
        
        // Display the calculated value, with the recommended amount of decimals.
        Serial.print(KLineKWP1281Lib::getMeasurementValue(i, amount_of_measurements, measurements, sizeof(measurements)),
                     KLineKWP1281Lib::getMeasurementDecimals(i, amount_of_measurements, measurements, sizeof(measurements)));
        
        // The function getMeasurementUnits() returns the same string that it's given. It's the same as units_string.
        Serial.print(' ');
        Serial.println(KLineKWP1281Lib::getMeasurementUnits(i, amount_of_measurements, measurements, sizeof(measurements), units_string, sizeof(units_string)));
      }
      break;
      
      // "Text" type
      case KLineKWP1281Lib::TEXT:
      {
        // This will hold the measurement's text.
        char text_string[16];
        
        // The function getMeasurementText() returns the same string that it's given. It's the same as text_string.
        Serial.println(KLineKWP1281Lib::getMeasurementText(i, amount_of_measurements, measurements, sizeof(measurements), text_string, sizeof(text_string)));
      }
      break;
      
      // Invalid measurement index
      case KLineKWP1281Lib::UNKNOWN:
        Serial.println("N/A");
        break;
    }
  }

  // Leave an empty line.
  Serial.println();
}