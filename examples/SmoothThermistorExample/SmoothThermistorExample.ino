/*
 * The components:
 * - Thermistor (here a 10K thermistor is used)
 * - Resistor (here a 10K resistor is used)
 * - Some wires
 *
 * The easy circuit:
 *
 *                  Analog pin 0
 *                        |
 *    5V |-----/\/\/\-----+-----/\/\/\-----| GND
 *
 *               ^                ^
 *        10K thermistor     10K resistor
 *
 * The advanced circuit:
 *
 *          AREF      Analog pin 0
 *           |              |
 *    3.3V |-+---/\/\/\-----+-----/\/\/\-----| GND
 *
 *                 ^                ^
 *          10K thermistor     10K resistor
 */

#include <SmoothThermistor.h>

// default configuration
smooththermistor::ThermistorConfig cfg;

// optional configuration changes
// cfg.adc_resolution = smooththermistor::AdcResolution::bit12;
// cfg.samples = 16;
// cfg.filter_mode = smooththermistor::FilterMode::moving_average;
// cfg.oversample_factor = 4;

// IMPORTANT: class is inside the smooththermistor namespace
smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() {
  Serial.begin(9600);

  // use AREF when using the advanced circuit
  therm.use_aref(true);
}

void loop() {
  Serial.print("Temperature = ");
  Serial.println(therm.temperature());

  delay(1000);
}
