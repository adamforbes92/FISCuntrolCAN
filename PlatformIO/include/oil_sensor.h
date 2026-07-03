#pragma once

/*
 * oil_sensor.h — VAG / Hella combined oil-level + oil-temperature sensor
 *
 * MK4 Golf "G266" sensor (Hella 6PR 008 079-xx, VAG 1J0 907 660x and related).
 * Single signal line carrying a free-running PWM square wave. Unlike the
 * BMW/Hella "PULS" oil-condition sensor (which sends a ~1 Hz Temp/Level/Diag
 * frame), this part outputs a CONTINUOUS ~18-30 Hz waveform whose period and
 * high-time both rise with oil temperature.
 *
 * Decoding (see oil_sensor.cpp + MK4GolfOilLevelSensor/readme.md):
 *
 *   - ~36-37 ms period (~27 Hz), ~9 ms high  -> ~20 C
 *   - ~53-55 ms period (~18 Hz), ~46 ms high -> ~100 C
 *
 *   Temperature is mapped from the PERIOD via a two-point linear fit
 *   (oilSensorTempPeriod* in config.h), then EMA-filtered.
 *
 *   Oil LEVEL is not yet calibrated — the reference captures were taken in
 *   water with the level pad dry. Level output is NAN until an oil-bath depth
 *   test is done and oilSensorLevelCalibrated is enabled in config.h.
 *
 * Storage convention:
 *   All shared state lives as globals declared in config.h / config.cpp
 *   (oilSensor*). The functions below are the ISR plumbing and a handful of
 *   small convenience getters.
 *
 * Compile with oilSensorRawLog = 1 to dump every decoded cycle to Serial and
 * retune the oilSensor* defines in config.h against a real sensor.
 */

#include <Arduino.h>

// Lifecycle
void setupOilSensor();          // attach interrupt, clear state
void serviceOilSensor();        // call from loop(); decodes new pulses

// Convenience getters — return NAN / false if no fresh frame
float oilSensor_getTemperatureC();
float oilSensor_getLevelMm();
float oilSensor_getLevelPercent();
bool  oilSensor_isPresent();              // edges seen recently
bool  oilSensor_hasValidReading();        // frame decoded recently

// ISR — installed by setupOilSensor()
void IRAM_ATTR oilSensorEdgeISR();
