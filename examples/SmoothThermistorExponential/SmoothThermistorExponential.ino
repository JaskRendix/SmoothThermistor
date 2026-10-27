#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg{
    .filter_mode = smooththermistor::FilterMode::exponential, .exp_alpha = 0.2};

// IMPORTANT: class is inside the smooththermistor namespace
smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() { Serial.begin(9600); }

void loop() {
  Serial.println(therm.temperature());
  delay(300);
}
