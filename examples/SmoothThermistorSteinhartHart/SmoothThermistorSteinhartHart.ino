#include <SmoothThermistor.h>

smooththermistor::ThermistorConfig cfg{.use_full_steinhart = true,
                                       .sh_a = 1.009249522e-03,
                                       .sh_b = 2.378405444e-04,
                                       .sh_c = 2.019202697e-07};

smooththermistor::SmoothThermistor therm(A0, cfg);

void setup() { Serial.begin(9600); }

void loop() {
  Serial.println(therm.temperature());
  delay(500);
}
