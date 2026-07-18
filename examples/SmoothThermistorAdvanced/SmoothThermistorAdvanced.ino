#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg;
smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() {
  Serial.begin(9600);

  cfg.adc_resolution = smooththermistor::AdcResolution::bit12;
  cfg.samples = 20;
  cfg.oversample_factor = 4;
  cfg.filter_mode = smooththermistor::FilterMode::moving_average;
  cfg.enable_noise_estimation = true;
  cfg.stability_threshold = 0.05;
}

void loop() {
  double t = therm.temperature();

  Serial.print("Temperature = ");
  Serial.println(t);

  Serial.print("Stable = ");
  Serial.println(therm.is_stable());

  Serial.print("Noise stddev = ");
  Serial.println(therm.last_noise_stddev());

  delay(1000);
}
