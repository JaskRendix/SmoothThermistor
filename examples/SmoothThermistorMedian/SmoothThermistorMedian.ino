#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg;
smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() {
  Serial.begin(9600);

  cfg.filter_mode = smooththermistor::FilterMode::median;
  cfg.samples = 15;
}

void loop() {
  Serial.println(therm.temperature());
  delay(500);
}
