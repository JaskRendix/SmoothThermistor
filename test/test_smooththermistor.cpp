#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "Arduino.h"
#include "SmoothThermistor.h"

using smooththermistor::AdcResolution;
using smooththermistor::FilterMode;
using smooththermistor::SmoothThermistor;
using smooththermistor::ThermistorConfig;

// -----------------------------------------------------------------------------
// Fake ADC helpers
// -----------------------------------------------------------------------------

std::uint16_t adc_mid(std::uint8_t) { return 512; }
std::uint16_t adc_low(std::uint8_t) { return 100; }
std::uint16_t adc_high(std::uint8_t) { return 900; }
std::uint16_t adc_zero(std::uint8_t) { return 0; }

// Sequence ADC for noise / stability tests
struct SeqADC {
  int base;
  int step;
  int jitter;
  int counter = 0;

  std::uint16_t operator()(std::uint8_t) {
    int v = base + step * counter;
    if (jitter != 0) {
      v += (counter % 2 ? jitter : -jitter);
    }
    counter++;
    return static_cast<std::uint16_t>(std::clamp(v, 0, 1023));
  }
};

// -----------------------------------------------------------------------------
// Basic functionality
// -----------------------------------------------------------------------------

TEST_CASE("Basic temperature calculation produces sane values") {
  ThermistorConfig cfg;
  SmoothThermistor therm(0, cfg, adc_mid);

  double t = therm.temperature();
  REQUIRE(std::isfinite(t));
  REQUIRE(t > -50.0);
  REQUIRE(t < 150.0);
}

TEST_CASE("ADC monotonicity: higher ADC → higher temperature") {
  ThermistorConfig cfg;

  SmoothThermistor t1(0, cfg, adc_low);
  SmoothThermistor t2(0, cfg, adc_high);

  REQUIRE(t2.temperature() > t1.temperature());
}

TEST_CASE("ADC zero does not crash and returns finite temperature") {
  ThermistorConfig cfg;
  SmoothThermistor therm(0, cfg, adc_zero);

  double t = therm.temperature();
  REQUIRE(std::isfinite(t));
}

// -----------------------------------------------------------------------------
// Filtering tests
// -----------------------------------------------------------------------------

TEST_CASE("Exponential filter responds gradually to changes") {
  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::exponential;
  cfg.exp_alpha = 0.5;

  int v = 400;
  auto adc = [&](uint8_t) { return static_cast<std::uint16_t>(v); };

  SmoothThermistor therm(0, cfg, adc);

  double t1 = therm.temperature();
  v = 600;
  double t2 = therm.temperature();
  double t3 = therm.temperature();

  REQUIRE(t2 != t1);
  REQUIRE(t3 != t2);
}

TEST_CASE("Moving average smooths abrupt jumps") {
  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::moving_average;

  int v = 400;
  auto adc = [&](uint8_t) { return static_cast<std::uint16_t>(v); };

  SmoothThermistor therm(0, cfg, adc);

  double t1 = therm.temperature();
  v = 600;
  double t2 = therm.temperature();
  double t3 = therm.temperature();
  double t4 = therm.temperature();

  REQUIRE((t4 >= t1)); // MA never decreases on a rising step
  REQUIRE((t4 <= t2)); // but it must still be smoothed
}

TEST_CASE("Median filter rejects single outliers") {
  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::median;

  int call = 0;
  auto adc = [&](uint8_t) {
    call++;
    if (call == 3)
      return static_cast<std::uint16_t>(1023); // spike
    return static_cast<std::uint16_t>(512);
  };

  SmoothThermistor therm(0, cfg, adc);

  double t1 = therm.temperature();
  double t2 = therm.temperature();
  double t3 = therm.temperature(); // spike
  double t4 = therm.temperature();

  REQUIRE(std::abs(t4 - t1) < 0.5);
}

// -----------------------------------------------------------------------------
// Oversampling
// -----------------------------------------------------------------------------

TEST_CASE("Oversampling reduces noise variation") {
  ThermistorConfig cfg;
  cfg.oversample_factor = 8;
  cfg.samples = 2;

  SeqADC noisy{500, 1, 20};
  SmoothThermistor therm(0, cfg, noisy);

  double t1 = therm.temperature();
  double t2 = therm.temperature();

  REQUIRE(std::abs(t1 - t2) < 3.0);
}

// -----------------------------------------------------------------------------
// Noise estimation
// -----------------------------------------------------------------------------

TEST_CASE("Noise estimation detects jitter") {
  ThermistorConfig cfg;
  cfg.enable_noise_estimation = true;
  cfg.filter_mode = FilterMode::none; // raw temperature

  // Clean signal: constant ADC
  SeqADC clean{500, 0, 0};

  // Noisy signal: extreme jitter + step change
  SeqADC noisy{500, 50, 400}; // step=50, jitter=400

  SmoothThermistor t_clean(0, cfg, clean);
  SmoothThermistor t_noisy(0, cfg, noisy);

  // Fill history buffer (16 samples)
  for (int i = 0; i < 32; ++i) {
    t_clean.temperature();
    t_noisy.temperature();
  }

  REQUIRE((t_noisy.last_noise_stddev() > t_clean.last_noise_stddev()));
  REQUIRE((t_noisy.last_noise_variance() > t_clean.last_noise_variance()));
}

// -----------------------------------------------------------------------------
// Stability detection
// -----------------------------------------------------------------------------

TEST_CASE("Stability detection reports stable when changes are small") {
  ThermistorConfig cfg;
  cfg.stability_threshold = 0.05;

  SeqADC stable_adc{500, 0, 1};
  SmoothThermistor therm(0, cfg, stable_adc);

  for (int i = 0; i < 20; ++i) {
    therm.temperature();
  }

  REQUIRE(therm.is_stable());
}

TEST_CASE("Stability detection reports unstable on large jumps") {
  ThermistorConfig cfg;
  cfg.stability_threshold = 0.05;

  int v = 400;
  auto adc = [&](uint8_t) { return static_cast<std::uint16_t>(v); };

  SmoothThermistor therm(0, cfg, adc);

  therm.temperature();
  v = 800;
  therm.temperature();

  REQUIRE_FALSE(therm.is_stable());
}

// -----------------------------------------------------------------------------
// Lookup table
// -----------------------------------------------------------------------------

TEST_CASE("Lookup table interpolation stays within bounds") {
  static double table[] = {10.0, 20.0, 30.0};

  ThermistorConfig cfg;
  cfg.use_lookup_table = true;
  cfg.lookup_table = table;
  cfg.lookup_size = 3;
  cfg.series_resistance = 10'000;

  SmoothThermistor therm(0, cfg, adc_mid);

  double t = therm.temperature();
  REQUIRE(t >= 10.0);
  REQUIRE(t <= 30.0);
}

// -----------------------------------------------------------------------------
// ADC resolution
// -----------------------------------------------------------------------------

TEST_CASE("Different ADC resolutions produce close but distinct temperatures") {
  ThermistorConfig cfg10;
  cfg10.adc_resolution = AdcResolution::bit10;

  ThermistorConfig cfg12;
  cfg12.adc_resolution = AdcResolution::bit12;

  auto adc = [&](uint8_t) { return static_cast<std::uint16_t>(512); };

  SmoothThermistor t10(0, cfg10, adc);
  SmoothThermistor t12(0, cfg12, adc);

  double temp10 = t10.temperature();
  double temp12 = t12.temperature();

  REQUIRE(std::isfinite(temp10));
  REQUIRE(std::isfinite(temp12));
  REQUIRE(temp10 != temp12);
  REQUIRE((temp10 > -50));
  REQUIRE((temp10 < 150));
  REQUIRE((temp12 > -50));
  REQUIRE((temp12 < 150));
}

// -----------------------------------------------------------------------------
// Steinhart–Hart
// -----------------------------------------------------------------------------

TEST_CASE("Steinhart–Hart produces valid temperature") {
  ThermistorConfig cfg;
  cfg.use_full_steinhart = true;
  cfg.sh_a = 1.009249522e-03;
  cfg.sh_b = 2.378405444e-04;
  cfg.sh_c = 2.019202697e-07;

  SmoothThermistor therm(0, cfg, adc_mid);

  double t = therm.temperature();
  REQUIRE(t > -50.0);
  REQUIRE(t < 150.0);
}

// -----------------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------------

TEST_CASE("Internal state updates when ADC changes") {
  ThermistorConfig cfg;

  int v = 500;
  auto adc = [&](uint8_t) { return static_cast<std::uint16_t>(v); };

  SmoothThermistor therm(0, cfg, adc);

  double a = therm.temperature();
  v = 600;
  double b = therm.temperature();

  REQUIRE(a != b);
}
