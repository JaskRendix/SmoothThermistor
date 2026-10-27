#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "Arduino.h"
#include "SmoothThermistor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

using smooththermistor::adc_max;
using smooththermistor::AdcResolution;
using smooththermistor::FilterMode;
using smooththermistor::LookupEntry;
using smooththermistor::SmoothThermistor;
using smooththermistor::ThermistorConfig;

// -----------------------------------------------------------------------------
// Fixed ADC Sources
// -----------------------------------------------------------------------------

std::uint16_t adc_mid(std::uint8_t) { return 512; }
std::uint16_t adc_low(std::uint8_t) { return 100; }
std::uint16_t adc_high(std::uint8_t) { return 900; }
std::uint16_t adc_zero(std::uint8_t) { return 0; }
std::uint16_t adc_max_10bit(std::uint8_t) { return 1023; }

std::uint16_t adc500(std::uint8_t) { return 500; }

static int g_adc_value = 400;

std::uint16_t adc_variable(std::uint8_t) {
  return static_cast<std::uint16_t>(g_adc_value);
}

// -----------------------------------------------------------------------------
// Median Spike Signal
// -----------------------------------------------------------------------------

static int g_median_call = 0;

std::uint16_t adc_median_spike(std::uint8_t) {
  ++g_median_call;

  if (g_median_call == 3)
    return 1023;

  return 512;
}

// -----------------------------------------------------------------------------
// Sequence ADC Generator
// -----------------------------------------------------------------------------

struct SeqADC {
  int base;
  int step;
  int jitter;
  int counter = 0;

  std::uint16_t next() {
    int value = base + step * counter;

    if (jitter != 0)
      value += (counter % 2 ? jitter : -jitter);

    ++counter;

    return static_cast<std::uint16_t>(std::clamp(value, 0, 1023));
  }
};

static SeqADC g_seq_clean{500, 0, 0};
static SeqADC g_seq_noisy{500, 1, 20};
static SeqADC g_seq_stable{500, 0, 1};

std::uint16_t seq_clean(std::uint8_t) { return g_seq_clean.next(); }

std::uint16_t seq_noisy(std::uint8_t) { return g_seq_noisy.next(); }

std::uint16_t seq_stable(std::uint8_t) { return g_seq_stable.next(); }

// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------

#define DRIFT_TICK_WINDOW(inst, samples, oversample)                           \
  for (std::uint32_t i = 0; i < (static_cast<std::uint32_t>(samples) *         \
                                 static_cast<std::uint32_t>(oversample));      \
       ++i) {                                                                  \
    (void)(inst).tick();                                                       \
    delay(1);                                                                  \
  }

// -----------------------------------------------------------------------------
// Configuration Validation
// -----------------------------------------------------------------------------

TEST_CASE("Default configuration validates") {
  ThermistorConfig cfg;
  REQUIRE(cfg.validate());
}

TEST_CASE("Invalid lookup table configuration fails validation") {
  ThermistorConfig cfg;

  cfg.use_lookup_table = true;
  cfg.lookup_table = nullptr;
  cfg.lookup_size = 0;

  REQUIRE_FALSE(cfg.validate());
}

TEST_CASE("Invalid exponential alpha fails validation") {
  ThermistorConfig cfg;

  cfg.filter_mode = FilterMode::exponential;
  cfg.exp_alpha = 5.0;

  REQUIRE_FALSE(cfg.validate());
}

TEST_CASE("Steinhart-Hart configuration validates") {
  ThermistorConfig cfg;

  cfg.use_full_steinhart = true;
  cfg.sh_a = 1.009249522e-03;
  cfg.sh_b = 2.378405444e-04;
  cfg.sh_c = 2.019202697e-07;

  REQUIRE(cfg.validate());
}

// -----------------------------------------------------------------------------
// ADC Helper
// -----------------------------------------------------------------------------

TEST_CASE("adc_max helper returns expected limits") {
  REQUIRE(adc_max(AdcResolution::bit10) == 1023);
  REQUIRE(adc_max(AdcResolution::bit12) == 4095);
}

// -----------------------------------------------------------------------------
// Constructor Fallback
// -----------------------------------------------------------------------------

TEST_CASE("Invalid configuration falls back safely") {
  ThermistorConfig cfg;

  cfg.samples = 0;
  cfg.oversample_factor = 0;

  SmoothThermistor therm(0, cfg, adc_mid);

  double t = therm.temperature();

  REQUIRE(std::isfinite(t));
}

// -----------------------------------------------------------------------------
// Non-blocking Engine
// -----------------------------------------------------------------------------

TEST_CASE("Asynchronous engine completes only after window is full") {
  ThermistorConfig cfg;
  cfg.samples = 5;
  cfg.oversample_factor = 2;

  SmoothThermistor therm(0, cfg, adc_mid);

  for (int i = 0; i < 9; ++i) {
    REQUIRE_FALSE(therm.tick());
    delay(1);
  }

  REQUIRE(therm.tick());
}

TEST_CASE("Invalid configuration falls back to defaults") {
  ThermistorConfig cfg;
  cfg.samples = 0;

  SmoothThermistor therm(0, cfg, adc_mid);

  REQUIRE(therm.tick());
  REQUIRE(std::isfinite(therm.temperature()));
}

// -----------------------------------------------------------------------------
// Physical Limits
// -----------------------------------------------------------------------------

TEST_CASE("ADC boundary conditions remain finite") {

  ThermistorConfig cfg;
  cfg.samples = 1;

  SECTION("ADC zero") {
    SmoothThermistor therm(0, cfg, adc_zero);

    REQUIRE(therm.tick());
    REQUIRE(std::isfinite(therm.temperature()));
  }

  SECTION("ADC maximum") {
    SmoothThermistor therm(0, cfg, adc_max_10bit);

    REQUIRE(therm.tick());
    REQUIRE(std::isfinite(therm.temperature()));
  }
}

// -----------------------------------------------------------------------------
// Basic Temperature Reading
// -----------------------------------------------------------------------------

TEST_CASE("Temperature reading is finite and sane") {

  ThermistorConfig cfg;
  cfg.samples = 1;

  SmoothThermistor therm(0, cfg, adc_mid);

  REQUIRE(therm.tick());

  double t = therm.temperature();

  REQUIRE(std::isfinite(t));
  REQUIRE(t > -100.0);
  REQUIRE(t < 300.0);
}

// -----------------------------------------------------------------------------
// Exponential Filter
// -----------------------------------------------------------------------------

TEST_CASE("Exponential filter responds gradually") {

  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::exponential;
  cfg.exp_alpha = 0.5;
  cfg.samples = 1;

  g_adc_value = 400;

  SmoothThermistor therm(0, cfg, adc_variable);

  REQUIRE(therm.tick());
  double t1 = therm.temperature();

  g_adc_value = 700;

  delay(1);
  REQUIRE(therm.tick());
  double t2 = therm.temperature();

  delay(1);
  REQUIRE(therm.tick());
  double t3 = therm.temperature();

  REQUIRE(t2 != t1);
  REQUIRE(t3 != t2);
}

// -----------------------------------------------------------------------------
// Moving Average
// -----------------------------------------------------------------------------

TEST_CASE("Moving average smooths changes") {

  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::moving_average;
  cfg.samples = 1;

  g_adc_value = 400;

  SmoothThermistor therm(0, cfg, adc_variable);

  REQUIRE(therm.tick());
  double t1 = therm.temperature();

  g_adc_value = 700;

  delay(1);
  REQUIRE(therm.tick());
  double t2 = therm.temperature();

  REQUIRE(t2 != t1);
}

// -----------------------------------------------------------------------------
// Median Filter
// -----------------------------------------------------------------------------

TEST_CASE("Median filter rejects spikes") {

  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::median;
  cfg.samples = 1;

  g_median_call = 0;

  SmoothThermistor therm(0, cfg, adc_median_spike);

  REQUIRE(therm.tick());
  double t1 = therm.temperature();

  delay(1);
  REQUIRE(therm.tick());

  delay(1);
  REQUIRE(therm.tick());

  delay(1);
  REQUIRE(therm.tick());

  double t4 = therm.temperature();

  REQUIRE(std::abs(t4 - t1) < 0.5);
}

// -----------------------------------------------------------------------------
// Oversampling
// -----------------------------------------------------------------------------

TEST_CASE("Oversampling reduces apparent variation") {

  ThermistorConfig cfg;
  cfg.samples = 2;
  cfg.oversample_factor = 8;

  g_seq_noisy = SeqADC{500, 1, 20};

  SmoothThermistor therm(0, cfg, seq_noisy);

  DRIFT_TICK_WINDOW(therm, cfg.samples, cfg.oversample_factor);

  double t1 = therm.temperature();

  DRIFT_TICK_WINDOW(therm, cfg.samples, cfg.oversample_factor);

  double t2 = therm.temperature();

  REQUIRE(std::abs(t1 - t2) < 3.0);
}

// -----------------------------------------------------------------------------
// Noise Estimation
// -----------------------------------------------------------------------------

TEST_CASE("Noise estimator reports larger noise for noisy signals") {

  ThermistorConfig cfg;
  cfg.enable_noise_estimation = true;
  cfg.samples = 1;

  g_seq_clean = SeqADC{500, 0, 0};
  g_seq_noisy = SeqADC{500, 50, 400};

  SmoothThermistor clean(0, cfg, seq_clean);
  SmoothThermistor noisy(0, cfg, seq_noisy);

  for (int i = 0; i < 20; ++i) {
    (void)clean.tick();
    (void)noisy.tick();
    delay(1);
  }

  REQUIRE(noisy.last_noise_stddev() > clean.last_noise_stddev());
}

TEST_CASE("Perfectly clean signal reports zero variance") {

  ThermistorConfig cfg;
  cfg.enable_noise_estimation = true;
  cfg.samples = 1;

  SmoothThermistor therm(0, cfg, adc_mid);

  for (int i = 0; i < 20; ++i) {
    (void)therm.tick();
    delay(1);
  }

  REQUIRE(therm.last_noise_stddev() == Catch::Approx(0.0).margin(1e-5));
}

// -----------------------------------------------------------------------------
// Stability Detection
// -----------------------------------------------------------------------------

TEST_CASE("Stable signal is detected as stable") {

  ThermistorConfig cfg;
  cfg.samples = 1;
  cfg.stability_threshold = 0.5;

  g_seq_stable = SeqADC{500, 0, 1};

  SmoothThermistor therm(0, cfg, seq_stable);

  for (int i = 0; i < 20; ++i) {
    (void)therm.tick();
    delay(1);
  }

  REQUIRE(therm.is_stable());
}

TEST_CASE("Large jump is detected as unstable") {

  ThermistorConfig cfg;
  cfg.samples = 1;
  cfg.stability_threshold = 0.05;

  g_adc_value = 400;

  SmoothThermistor therm(0, cfg, adc_variable);

  REQUIRE(therm.tick());

  g_adc_value = 800;

  delay(1);
  REQUIRE(therm.tick());

  REQUIRE_FALSE(therm.is_stable());
}

// -----------------------------------------------------------------------------
// Lookup Tables
// -----------------------------------------------------------------------------

TEST_CASE("Lookup table interpolates correctly") {

  static LookupEntry table[] = {{0, 10.0}, {1023, 30.0}};

  ThermistorConfig cfg;

  cfg.use_lookup_table = true;
  cfg.lookup_table = table;
  cfg.lookup_size = 2;
  cfg.samples = 1;

  SmoothThermistor therm(0, cfg, adc_mid);

  REQUIRE(therm.tick());

  REQUIRE(therm.temperature() == Catch::Approx(20.0).margin(0.1));
}

TEST_CASE("Lookup table returns exact node values") {

  static LookupEntry table[] = {{0, 10.0}, {512, 20.0}, {1023, 30.0}};

  ThermistorConfig cfg;

  cfg.use_lookup_table = true;
  cfg.lookup_table = table;
  cfg.lookup_size = 3;
  cfg.samples = 1;

  SmoothThermistor therm(0, cfg, adc_mid);

  REQUIRE(therm.tick());

  REQUIRE(therm.temperature() == Catch::Approx(20.0));
}

TEST_CASE("Lookup table clips at boundaries") {

  static LookupEntry table[] = {{200, -10.0}, {500, 25.0}, {800, 85.0}};

  ThermistorConfig cfg;

  cfg.use_lookup_table = true;
  cfg.lookup_table = table;
  cfg.lookup_size = 3;
  cfg.samples = 1;

  SECTION("Below range") {

    SmoothThermistor therm(0, cfg, adc_low);

    REQUIRE(therm.tick());

    REQUIRE(therm.temperature() == Catch::Approx(-10.0));
  }

  SECTION("Above range") {

    SmoothThermistor therm(0, cfg, adc_high);

    REQUIRE(therm.tick());

    REQUIRE(therm.temperature() == Catch::Approx(85.0));
  }
}

TEST_CASE("Duplicate ADC entries do not cause division by zero") {

  static LookupEntry table[] = {{500, 20.0}, {500, 21.0}, {700, 40.0}};

  ThermistorConfig cfg;

  cfg.use_lookup_table = true;
  cfg.lookup_table = table;
  cfg.lookup_size = 3;
  cfg.samples = 1;

  SmoothThermistor therm(0, cfg, adc500);

  REQUIRE(therm.tick());

  REQUIRE(std::isfinite(therm.temperature()));
}

// -----------------------------------------------------------------------------
// Steinhart-Hart
// -----------------------------------------------------------------------------

TEST_CASE("Steinhart-Hart produces finite temperature") {

  ThermistorConfig cfg;

  cfg.use_full_steinhart = true;
  cfg.sh_a = 1.009249522e-03;
  cfg.sh_b = 2.378405444e-04;
  cfg.sh_c = 2.019202697e-07;
  cfg.samples = 1;

  SmoothThermistor therm(0, cfg, adc_mid);

  REQUIRE(therm.tick());

  double t = therm.temperature();

  REQUIRE(std::isfinite(t));
  REQUIRE(t > -100.0);
  REQUIRE(t < 300.0);
}