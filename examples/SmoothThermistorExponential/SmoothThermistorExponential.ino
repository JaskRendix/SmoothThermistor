#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg;

// Configure exponential smoothing
void setup() {
  Serial.begin(9600);

  cfg.filter_mode = smooththermistor::FilterMode::exponential;
  cfg.exp_alpha = 0.2;
}

// IMPORTANT: class is inside the smooththermistor namespace
smooththermistor::SmoothThermistor therm(A0, cfg);

void loop() {
  Serial.println(therm.temperature());
  delay(300);
}
