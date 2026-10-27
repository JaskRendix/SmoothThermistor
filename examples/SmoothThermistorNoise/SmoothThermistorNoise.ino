#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg{.samples = 20,
                                       .enable_noise_estimation = true};

smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() { Serial.begin(9600); }

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
