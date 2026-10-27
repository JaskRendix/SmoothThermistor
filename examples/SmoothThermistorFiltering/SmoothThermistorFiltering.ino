#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg{
    .samples = 20, .filter_mode = smooththermistor::FilterMode::moving_average};

smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() { Serial.begin(9600); }

void loop() {
  double t = therm.temperature();

  Serial.print("Filtered temperature = ");
  Serial.println(t);

  delay(500);
}
