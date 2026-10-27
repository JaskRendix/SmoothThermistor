#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg{.samples = 10, .oversample_factor = 8};

smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() { Serial.begin(9600); }

void loop() {
  Serial.println(therm.temperature());
  delay(500);
}
