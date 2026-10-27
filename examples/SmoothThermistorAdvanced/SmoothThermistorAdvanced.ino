#include <SmoothThermistor.h>

using namespace smooththermistor;

ThermistorConfig cfg{.samples = 20,
                     .oversample_factor = 4,
                     .adc_resolution = AdcResolution::bit12,
                     .filter_mode = FilterMode::moving_average,
                     .enable_noise_estimation = true,
                     .stability_threshold = 0.05};

SmoothThermistor therm(A0, cfg);

void setup() { Serial.begin(9600); }

void loop() {
  const double t = therm.temperature();

  Serial.print("Temperature = ");
  Serial.print(t, 2);
  Serial.println(" °C");

  Serial.print("Stable = ");
  Serial.println(therm.is_stable() ? "yes" : "no");

  Serial.print("Noise stddev = ");
  Serial.println(therm.last_noise_stddev(), 3);

  delay(1000);
}
