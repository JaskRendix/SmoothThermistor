#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg;
smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() {
  Serial.begin(9600);

  cfg.samples = 10;
  cfg.oversample_factor = 8;
}

void loop() {
  Serial.println(therm.temperature());
  delay(500);
}
