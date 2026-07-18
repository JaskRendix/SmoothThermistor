#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg;
smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() {
  Serial.begin(9600);

  cfg.filter_mode = smooththermistor::FilterMode::moving_average;
  cfg.samples = 20;
}

void loop() {
  double t = therm.temperature();
  Serial.print("Filtered temperature = ");
  Serial.println(t);
  delay(500);
}
