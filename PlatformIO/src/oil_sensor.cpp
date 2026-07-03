/*
 * oil_sensor.cpp — VAG / Hella MK4 Golf combined oil-level + temperature
 * sensor decoder (G266; Hella 6PR 008 079-xx, VAG 1J0 907 660x).
 *
 * SIGNAL MODEL — CONTINUOUS PWM (verified against scope captures):
 *
 *   The sensor drives a single signal line with a free-running square wave at
 *   roughly 18-30 Hz (~36-55 ms period). BOTH the period and the high-time
 *   grow as the sensor heats up:
 *
 *       ~20 C  : period ~36-37 ms (~27 Hz),  high ~9 ms
 *       ~100 C : period ~53-55 ms (~18 Hz),  high ~46 ms
 *
 *   There is NO ~780 ms sync gap and NO 3-pulse (Temp/Level/Diag) frame — that
 *   is a different part (the BMW/Hella "PULS" oil-condition sensor). This
 *   decoder therefore measures every cycle and maps PERIOD -> temperature.
 *
 *   Level is NOT decoded yet: the reference captures were taken in water with
 *   the level pad dry, so there is no valid pulse-width -> level mapping. Level
 *   output stays NAN until oilSensorLevelCalibrated is set (see config.h).
 *
 * Implementation:
 *   - The ISR timestamps every edge. On a falling edge it records the high-time
 *     of the pulse that just ended; on the next rising edge it has a complete
 *     cycle (rise-to-rise period + that high-time) and flags it for the
 *     foreground.
 *   - serviceOilSensor() lifts the cycle out under a brief critical section,
 *     range-checks it, converts period to temperature, and applies an
 *     exponential moving-average filter to smooth the ~20 Hz stream.
 *
 *   Compile with oilSensorRawLog = 1 to dump every cycle (period / high / low /
 *   duty / temperature) to Serial — use this on the car to refit the
 *   oilSensorTempPeriod* points and to gather data for the level calibration.
 */

#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "oil_sensor.h"

// ---------------------------------------------------------------------------
// ISR-private state (only touched from oilSensorEdgeISR)
// ---------------------------------------------------------------------------
static volatile uint32_t isrLastRiseUs = 0;
static volatile uint32_t isrLastFallUs = 0;
static volatile uint32_t isrLastHighUs = 0;
static volatile bool     isrHaveRise   = false;

// ---------------------------------------------------------------------------
// Foreground decoder state (only touched from serviceOilSensor)
// ---------------------------------------------------------------------------
static float oilSensorTempFilt = NAN;   // EMA-filtered temperature

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
// Plain two-point linear map — NOT clamped, so temperature extrapolates
// sensibly below 20 C / above 100 C.
static float oilSensorMap(float x, float x0, float y0, float x1, float y1) {
    if (x1 == x0) return y0;
    return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
}

static bool oilSensorReadingFresh() {
    if (oilSensorLastFrameMs == 0) return false;
    return (millis() - oilSensorLastFrameMs) < oilSensorStaleMs;
}

// ---------------------------------------------------------------------------
// ISR — fast, no floats, no Serial
// ---------------------------------------------------------------------------
void IRAM_ATTR oilSensorEdgeISR() {
    uint32_t nowUs = micros();
    uint8_t  level = (uint8_t)digitalRead(oilSensorPin);
    oilSensorLastEdgeMs = millis();

    if (level) {
        // Rising edge — a full cycle (previous rise -> this rise) just closed.
        if (isrHaveRise) {
            uint32_t periodUs = nowUs - isrLastRiseUs;
            oilSensorLastPeriodUs = periodUs;
            oilSensorLastPulseUs  = isrLastHighUs;
            oilSensorLastGapUs    = (periodUs > isrLastHighUs)
                                        ? (periodUs - isrLastHighUs) : 0;
            oilSensorPulseReady   = true;
        }
        isrLastRiseUs = nowUs;
        isrHaveRise   = true;
    } else {
        // Falling edge — close the high portion of the current cycle.
        if (isrHaveRise) {
            isrLastHighUs = nowUs - isrLastRiseUs;
        }
        isrLastFallUs = nowUs;
    }
}

// ---------------------------------------------------------------------------
// Foreground service — consume one completed cycle at a time
// ---------------------------------------------------------------------------
static void oilSensorDecodeCycle(uint32_t periodUs, uint32_t highUs) {
    // Reject glitches and lost cycles.
    if (periodUs < oilSensorPeriodMinUs || periodUs > oilSensorPeriodMaxUs ||
        highUs   < oilSensorPulseMinUs  || highUs   > oilSensorPulseMaxUs  ||
        highUs >= periodUs) {
        oilSensorErrorCount++;
#if oilSensorRawLog
        LOGIO("[OIL] period=%lu us  high=%lu us  -> OUT OF RANGE",
                      (unsigned long)periodUs, (unsigned long)highUs);
#endif
        return;
    }

    // Period -> temperature (linear, extrapolated), then EMA smoothing.
    float tInst = oilSensorMap((float)periodUs,
                               oilSensorTempPeriod0Us, oilSensorTempPeriod0C,
                               oilSensorTempPeriod1Us, oilSensorTempPeriod1C);
    if (isnan(oilSensorTempFilt)) {
        oilSensorTempFilt = tInst;
    } else {
        oilSensorTempFilt += oilSensorFilterAlpha * (tInst - oilSensorTempFilt);
    }
    oilSensorTemperatureC = oilSensorTempFilt;

#if oilSensorLevelCalibrated
    oilSensorLevelMm = oilSensorMap((float)highUs,
                                    oilSensorLevelPulse0Us, oilSensorLevelPulse0Mm,
                                    oilSensorLevelPulse1Us, oilSensorLevelPulse1Mm);
#else
    oilSensorLevelMm = NAN;   // not calibrated — see config.h
#endif

    oilSensorFrameCount++;
    oilSensorLastFrameMs = millis();

#if oilSensorRawLog
    float dutyPct = 100.0f * (float)highUs / (float)periodUs;
    LOGIO("[OIL] period=%lu us (%.1f Hz)  high=%lu us  low=%lu us  duty=%.1f%%  T=%.1fC  cyc=%lu err=%lu",
                  (unsigned long)periodUs,
                  (double)(1.0e6 / (double)periodUs),
                  (unsigned long)highUs,
                  (unsigned long)(periodUs - highUs),
                  (double)dutyPct,
                  (double)oilSensorTemperatureC,
                  (unsigned long)oilSensorFrameCount,
                  (unsigned long)oilSensorErrorCount);
#endif
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void setupOilSensor() {
#if hasOilSensor
    pinMode(oilSensorPin, INPUT);

    isrLastRiseUs = 0;
    isrLastFallUs = 0;
    isrLastHighUs = 0;
    isrHaveRise   = false;

    oilSensorTempFilt = NAN;

    oilSensorLastPeriodUs  = 0;
    oilSensorLastPulseUs   = 0;
    oilSensorLastGapUs     = 0;
    oilSensorPulseReady    = false;
    oilSensorLastEdgeMs    = 0;
    oilSensorTemperatureC  = NAN;
    oilSensorLevelMm       = NAN;
    oilSensorFrameCount    = 0;
    oilSensorErrorCount    = 0;
    oilSensorLastFrameMs   = 0;

    attachInterrupt(digitalPinToInterrupt(oilSensorPin),
                    oilSensorEdgeISR, CHANGE);

    LOGIO("oilSensor: attached to GPIO%d (raw_log=%d)",
          (int)oilSensorPin, (int)oilSensorRawLog);
#endif
}

void serviceOilSensor() {
#if hasOilSensor
    if (!oilSensorPulseReady) {
        return;
    }

    // Lift the latest cycle out under a brief critical section so the ISR
    // can't half-overwrite the pair while we're reading it.
    noInterrupts();
    uint32_t periodUs = oilSensorLastPeriodUs;
    uint32_t highUs   = oilSensorLastPulseUs;
    oilSensorPulseReady = false;
    interrupts();

    oilSensorDecodeCycle(periodUs, highUs);
#endif
}

float oilSensor_getTemperatureC() {
    if (!oilSensorReadingFresh()) return NAN;
    return oilSensorTemperatureC;
}

float oilSensor_getLevelMm() {
    if (!oilSensorReadingFresh()) return NAN;
    return oilSensorLevelMm;
}

float oilSensor_getLevelPercent() {
    float mm = oilSensor_getLevelMm();
    if (isnan(mm)) return NAN;
    float pct = 100.0f * mm / (float)oilSensorLevelSpanMm;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

bool oilSensor_isPresent() {
    if (oilSensorLastEdgeMs == 0) return false;
    return (millis() - oilSensorLastEdgeMs) < oilSensorStaleMs;
}

bool oilSensor_hasValidReading() {
    return oilSensorReadingFresh();
}
