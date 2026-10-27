#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg{
    .samples = 15, .filter_mode = smooththermistor::FilterMode::median};

smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() { Serial.begin(9600); }

void loop() {
  Serial.println(therm.temperature());
  delay(500);
}
