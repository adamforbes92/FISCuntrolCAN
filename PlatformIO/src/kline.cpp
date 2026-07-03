/*
 * kline.cpp — KLineKWP1281Lib integration
 *
 * Hardware Serial2 on the ESP32 is used as the K-line UART.
 */

#include "kline.h"
#include "config.h"
#include "cards.h"

#include <KLineKWP1281Lib_ESP32.h>

// ---------------------------------------------------------------------------
// Low-level Serial callbacks required by the library
// ---------------------------------------------------------------------------
void beginFunction(unsigned long baud) {
    K_line.begin(baud, SERIAL_8N1, K_RX, K_TX);
}

void endFunction() {
    K_line.end();
}

void sendFunction(uint8_t data) {
    K_line.write(data);
}

bool receiveFunction(uint8_t* data, unsigned long timeout) {
    unsigned long start = millis();
    while (!K_line.available()) {
        if (millis() - start > timeout) return 0;
    }
    *data = K_line.read();
    return 1;
}

// ---------------------------------------------------------------------------
// Library instance
// ---------------------------------------------------------------------------
#if enableDebug
KLineKWP1281Lib diag(beginFunction, endFunction, sendFunction, receiveFunction,
                     K_TX, is_full_duplex);
#else
KLineKWP1281Lib diag(beginFunction, endFunction, sendFunction, receiveFunction,
                     K_TX, is_full_duplex);
#endif

// ---------------------------------------------------------------------------
// Measurement buffers
// ---------------------------------------------------------------------------
static uint8_t measurement_buffer[80];
static uint8_t measurement_body_buffer[4];

// ---------------------------------------------------------------------------
// Read a single measurement group and populate fisLine[]
// ---------------------------------------------------------------------------
void showMeasurements(uint8_t group) {
    uint8_t amount_of_measurements      = 0;
    bool    received_group_header       = false;
    uint8_t amount_of_measurements_in_header = 0;

    diag.update();

    beginCard(CARD_SOURCE_KLINE, "K-LINE");
    {
        char blockBuf[20];
        snprintf(blockBuf, sizeof(blockBuf), "BLOCK: %u", group);
        setCardLine(0, String(blockBuf));
    }

    uint8_t cardLine = 1;

    for (uint8_t attempt = 1; attempt <= 1; attempt++) {
        KLineKWP1281Lib::executionStatus readGroup_status;

        if (!received_group_header) {
            readGroup_status = diag.readGroup(amount_of_measurements, group,
                                              measurement_buffer, sizeof(measurement_buffer));
        } else {
            readGroup_status = diag.readGroup(amount_of_measurements, group,
                                              measurement_body_buffer, sizeof(measurement_body_buffer));
        }

        switch (readGroup_status) {
            case KLineKWP1281Lib::ERROR:
                LOGKLN("K-line: error reading group!");
                return;

            case KLineKWP1281Lib::FAIL:
                LOGKLN("K-line: group %d does not exist!", group);
                return;

            case KLineKWP1281Lib::GROUP_BASIC_SETTINGS:
                if (received_group_header) {
                    LOGKLN("K-line: unexpected basic settings in body read");
                    return;
                }
                LOGKLN("K-line: basic settings group %d", group);
                continue;

            case KLineKWP1281Lib::GROUP_HEADER:
                if (received_group_header) {
                    LOGKLN("K-line: unexpected header in body read");
                    return;
                }
                received_group_header            = true;
                amount_of_measurements_in_header = amount_of_measurements;
                attempt--;
                continue;

            case KLineKWP1281Lib::GROUP_BODY:
                if (!received_group_header) {
                    LOGKLN("K-line: body received without prior header");
                    return;
                }
                break;

            case KLineKWP1281Lib::SUCCESS:
                break;
        }

        lastKlineTransmission = millis();

        LOGKLN("K-line group %d:", group);

        for (uint8_t i = 0; i < amount_of_measurements; i++) {
            KLineKWP1281Lib::measurementType measurement_type;

            if (!received_group_header) {
                measurement_type = KLineKWP1281Lib::getMeasurementType(
                    i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer));
            } else {
                measurement_type = KLineKWP1281Lib::getMeasurementTypeFromHeader(
                    i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer));
            }

            switch (measurement_type) {
                case KLineKWP1281Lib::VALUE: {
                    char     units_string[16] = {};
                    double   value            = 0.0;
                    uint8_t  decimals         = 0;

                    if (!received_group_header) {
                        value    = KLineKWP1281Lib::getMeasurementValue(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer));
                        KLineKWP1281Lib::getMeasurementUnits(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer), units_string, sizeof(units_string));
                        decimals = KLineKWP1281Lib::getMeasurementDecimals(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer));
                    } else {
                        value    = KLineKWP1281Lib::getMeasurementValueFromHeaderBody(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer), amount_of_measurements, measurement_body_buffer, sizeof(measurement_body_buffer));
                        KLineKWP1281Lib::getMeasurementUnitsFromHeaderBody(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer), amount_of_measurements, measurement_body_buffer, sizeof(measurement_body_buffer), units_string, sizeof(units_string));
                        decimals = KLineKWP1281Lib::getMeasurementDecimalsFromHeader(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer));
                    }
                    (void)decimals;
                    LOGKLN(" %f %s", value, units_string);
                    if (cardLine < 8) {
                        setCardLine(cardLine++, String(value) + " " + String(units_string));
                    }
                    break;
                }

                case KLineKWP1281Lib::TEXT: {
                    char text_string[16] = {};
                    if (!received_group_header) {
                        KLineKWP1281Lib::getMeasurementText(i, amount_of_measurements, measurement_buffer, sizeof(measurement_buffer), text_string, sizeof(text_string));
                    } else {
                        KLineKWP1281Lib::getMeasurementTextFromHeaderBody(i, amount_of_measurements_in_header, measurement_buffer, sizeof(measurement_buffer), amount_of_measurements, measurement_body_buffer, sizeof(measurement_body_buffer), text_string, sizeof(text_string));
                    }
                    LOGKLN(" %s", text_string);
                    if (cardLine < 8) {
                        setCardLine(cardLine++, String(text_string));
                    }
                    break;
                }

                case KLineKWP1281Lib::UNKNOWN:
                    LOGKLN(" N/A");
                    if (cardLine < 8) {
                        setCardLine(cardLine++, "N/A");
                    }
                    break;
            }
        }
        LOGKLN("");
        commitCard();
    }
}

// ---------------------------------------------------------------------------
// Fault code operations
// ---------------------------------------------------------------------------
int readKlineFaults(KlineFaultEntry* outEntries, uint8_t maxEntries) {
    if (!isConnectedK) return -1;

    static uint8_t fault_buffer[128];
    uint8_t amount = 0;

    diag.update();
    const KLineKWP1281Lib::executionStatus status =
        diag.readFaults(amount, fault_buffer, sizeof(fault_buffer));

    if (status == KLineKWP1281Lib::ERROR || status == KLineKWP1281Lib::FAIL) {
        return -1;
    }

    const uint8_t count = (amount < maxEntries) ? amount : maxEntries;
    for (uint8_t i = 0; i < count; i++) {
        outEntries[i].code = KLineKWP1281Lib::getFaultCode(i, amount, fault_buffer, sizeof(fault_buffer));
        KLineKWP1281Lib::getFaultDescription(i, amount, fault_buffer, sizeof(fault_buffer),
                                             outEntries[i].description, sizeof(outEntries[i].description));
        bool intermittent = false;
        KLineKWP1281Lib::getFaultElaboration(intermittent, i, amount, fault_buffer, sizeof(fault_buffer),
                                             outEntries[i].elaboration, sizeof(outEntries[i].elaboration));
        outEntries[i].isIntermittent = intermittent;
    }
    return (int)count;
}

bool clearKlineFaults() {
    if (!isConnectedK) return false;
    diag.update();
    return diag.clearFaults() == KLineKWP1281Lib::SUCCESS;
}
