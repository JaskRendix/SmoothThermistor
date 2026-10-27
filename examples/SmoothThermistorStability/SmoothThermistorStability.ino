#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg{.stability_threshold = 0.05};

smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() { Serial.begin(9600); }

void loop() {
  double t = therm.temperature();

  Serial.print("Temperature = ");
  Serial.print(t);

  Serial.print("  Stable = ");
  Serial.println(therm.is_stable());

  delay(500);
}
