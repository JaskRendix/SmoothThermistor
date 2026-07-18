#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg;
smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() {
  Serial.begin(9600);

  cfg.enable_noise_estimation = true;
  cfg.samples = 20;
}

void loop() {
  double t = therm.temperature();

  Serial.print("Temperature = ");
  Serial.println(t);

  Serial.print("Noise stddev = ");
  Serial.println(therm.last_noise_stddev());

  Serial.print("Noise variance = ");
  Serial.println(therm.last_noise_variance());

  delay(1000);
}
