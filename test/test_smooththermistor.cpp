#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "Arduino.h"
#include "SmoothThermistor.h"
#include <algorithm>
#include <cmath>

using smooththermistor::AdcResolution;
using smooththermistor::FilterMode;
using smooththermistor::LookupEntry;
using smooththermistor::SmoothThermistor;
using smooththermistor::ThermistorConfig;

// -----------------------------------------------------------------------------
// Fake ADC helpers (simple function pointers)
// -----------------------------------------------------------------------------

std::uint16_t adc_mid(std::uint8_t) { return 512; }
std::uint16_t adc_low(std::uint8_t) { return 100; }
std::uint16_t adc_high(std::uint8_t) { return 900; }
std::uint16_t adc_zero(std::uint8_t) { return 0; }
std::uint16_t adc_max_10bit(std::uint8_t) { return 1023; }

// -----------------------------------------------------------------------------
// Global state helpers for tests that need changing signals
// -----------------------------------------------------------------------------

static int g_adc_value = 400;
std::uint16_t adc_variable(std::uint8_t) {
  return static_cast<std::uint16_t>(g_adc_value);
}

static int g_median_call = 0;
std::uint16_t adc_median_spike(std::uint8_t) {
  g_median_call++;
  if (g_median_call == 3)
    return static_cast<std::uint16_t>(1023);
  return static_cast<std::uint16_t>(512);
}

static int g_dynamic_signal = 300;
std::uint16_t dynamic_adc(std::uint8_t) {
  return static_cast<std::uint16_t>(g_dynamic_signal);
}

static int g_stepped_signal = 500;
std::uint16_t stepped_adc(std::uint8_t) {
  return static_cast<std::uint16_t>(g_stepped_signal);
}

// -----------------------------------------------------------------------------
// Sequence ADC for noise / stability tests (wrapped as functions)
// -----------------------------------------------------------------------------

struct SeqADC {
  int base;
  int step;
  int jitter;
  int counter = 0;

  std::uint16_t next() {
    int v = base + step * counter;
    if (jitter != 0) {
      v += (counter % 2 ? jitter : -jitter);
    }
    counter++;
    return static_cast<std::uint16_t>(std::clamp(v, 0, 1023));
  }
};

static SeqADC g_seq_noisy{500, 1, 20};
std::uint16_t seq_noisy(std::uint8_t) { return g_seq_noisy.next(); }

static SeqADC g_seq_clean{500, 0, 0};
std::uint16_t seq_clean(std::uint8_t) { return g_seq_clean.next(); }

static SeqADC g_seq_stable{500, 0, 1};
std::uint16_t seq_stable(std::uint8_t) { return g_seq_stable.next(); }

// Helper macro to flush samples through the non-blocking engine seamlessly
#define DRIFT_TICK_WINDOW(therminstance, samples, oversample)                  \
  for (std::uint32_t i = 0; i < (static_cast<std::uint32_t>(samples) *         \
                                 static_cast<std::uint32_t>(oversample));      \
       ++i) {                                                                  \
    (therminstance).tick();                                                    \
    delay(1);                                                                  \
  }

// -----------------------------------------------------------------------------
// Asynchronous & Boundary Engine Tests
// -----------------------------------------------------------------------------

TEST_CASE("Asynchronous non-blocking tick engine window lifecycle") {
  ThermistorConfig cfg;
  cfg.samples = 5;
  cfg.oversample_factor = 2; // Total 10 hardware samples required

  SmoothThermistor therm(0, cfg, adc_mid);

  // First 9 updates must register false (accumulation in progress)
  for (int i = 0; i < 9; ++i) {
    REQUIRE_FALSE(therm.tick());
    delay(1);
  }

  // Complete the 10th background milestone
  REQUIRE(therm.tick());
}

TEST_CASE("Zero configuration window sizes drop safely") {
  ThermistorConfig cfg;
  cfg.samples = 0; // Malformed configuration entry

  SmoothThermistor therm(0, cfg, adc_mid);
  REQUIRE_FALSE(therm.tick());
}

TEST_CASE("Physical fault limits do not induce mathematical singularities") {
  ThermistorConfig cfg;
  cfg.samples = 1;

  SECTION("Sensor Short Circuit (ADC = 0)") {
    SmoothThermistor therm(0, cfg, adc_zero);
    REQUIRE(therm.tick());
    REQUIRE(std::isfinite(therm.temperature()));
  }

  SECTION("Sensor Disconnected / Open Circuit (ADC = Max)") {
    SmoothThermistor therm(0, cfg, adc_max_10bit);
    REQUIRE(therm.tick());
    REQUIRE(std::isfinite(therm.temperature()));
  }
}

// -----------------------------------------------------------------------------
// Basic functionality
// -----------------------------------------------------------------------------

TEST_CASE("Basic temperature calculation produces sane values") {
  ThermistorConfig cfg;
  cfg.samples = 1;
  SmoothThermistor therm(0, cfg, adc_mid);

  REQUIRE(therm.tick());
  double t = therm.temperature();
  REQUIRE(std::isfinite(t));
  REQUIRE(t > -50.0);
  REQUIRE(t < 150.0);
}

TEST_CASE("ADC monotonicity: higher ADC -> higher temperature") {
  ThermistorConfig cfg;
  cfg.samples = 1;

  SmoothThermistor t1(0, cfg, adc_low);
  SmoothThermistor t2(0, cfg, adc_high);

  REQUIRE(t1.tick());
  REQUIRE(t2.tick());

  REQUIRE(t2.temperature() > t1.temperature());
}

// -----------------------------------------------------------------------------
// Filtering tests
// -----------------------------------------------------------------------------

TEST_CASE("Exponential filter responds gradually to changes") {
  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::exponential;
  cfg.exp_alpha = 0.5;
  cfg.samples = 1;

  g_adc_value = 400;
  SmoothThermistor therm(0, cfg, adc_variable);

  REQUIRE(therm.tick());
  double t1 = therm.temperature();

  g_adc_value = 600;
  delay(1);
  REQUIRE(therm.tick());
  double t2 = therm.temperature();

  delay(1);
  REQUIRE(therm.tick());
  double t3 = therm.temperature();

  REQUIRE(t2 != t1);
  REQUIRE(t3 != t2);
}

TEST_CASE("Moving average smooths abrupt jumps") {
  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::moving_average;
  cfg.samples = 1;

  g_adc_value = 400;
  SmoothThermistor therm(0, cfg, adc_variable);

  REQUIRE(therm.tick());
  double t1 = therm.temperature();

  g_adc_value = 600;
  delay(1);
  REQUIRE(therm.tick());
  double t2 = therm.temperature();

  delay(1);
  REQUIRE(therm.tick());
  double t3 = therm.temperature();

  delay(1);
  REQUIRE(therm.tick());
  double t4 = therm.temperature();

  // Qualitative smoothing guarantees:
  REQUIRE(t2 > t1);  // moved toward the jump
  REQUIRE(t3 >= t2); // continued moving smoothly
  REQUIRE(t4 >= t3); // still moving smoothly
}

TEST_CASE("Median filter completely isolates and flattens massive spikes") {
  ThermistorConfig cfg;
  cfg.filter_mode = FilterMode::median;
  cfg.samples = 1;

  g_median_call = 0;
  SmoothThermistor therm(0, cfg, adc_median_spike);

  REQUIRE(therm.tick());
  double t1 = therm.temperature();
  delay(1);
  REQUIRE(therm.tick());
  double t2 = therm.temperature();
  delay(1);
  REQUIRE(therm.tick());
  double t3 = therm.temperature();
  delay(1);
  REQUIRE(therm.tick());
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

  g_seq_noisy = SeqADC{500, 1, 20};
  SmoothThermistor therm(0, cfg, seq_noisy);

  DRIFT_TICK_WINDOW(therm, cfg.samples, cfg.oversample_factor);
  double t1 = therm.temperature();

  DRIFT_TICK_WINDOW(therm, cfg.samples, cfg.oversample_factor);
  double t2 = therm.temperature();

  REQUIRE(std::abs(t1 - t2) < 3.0);
}

// -----------------------------------------------------------------------------
// Noise estimation
// -----------------------------------------------------------------------------

TEST_CASE("Noise estimation detects jitter") {
  ThermistorConfig cfg;
  cfg.enable_noise_estimation = true;
  cfg.filter_mode = FilterMode::none;
  cfg.samples = 1;

  g_seq_clean = SeqADC{500, 0, 0};
  g_seq_noisy = SeqADC{500, 50, 400};

  SmoothThermistor t_clean(0, cfg, seq_clean);
  SmoothThermistor t_noisy(0, cfg, seq_noisy);

  for (int i = 0; i < 20; ++i) {
    t_clean.tick();
    t_noisy.tick();
    delay(1);
  }

  REQUIRE(t_noisy.last_noise_stddev() > t_clean.last_noise_stddev());
}

// -----------------------------------------------------------------------------
// Stability detection
// -----------------------------------------------------------------------------

TEST_CASE("Stability detection reports stable when changes are small") {
  ThermistorConfig cfg;
  cfg.stability_threshold = 0.5;
  cfg.samples = 1;

  g_seq_stable = SeqADC{500, 0, 1};
  SmoothThermistor therm(0, cfg, seq_stable);

  for (int i = 0; i < 20; ++i) {
    therm.tick();
    delay(1);
  }

  REQUIRE(therm.is_stable());
}

TEST_CASE("Stability detection reports unstable on large jumps") {
  ThermistorConfig cfg;
  cfg.stability_threshold = 0.05;
  cfg.samples = 1;

  g_adc_value = 400;
  SmoothThermistor therm(0, cfg, adc_variable);

  REQUIRE(therm.tick());
  g_adc_value = 800;
  delay(1);
  REQUIRE(therm.tick());

  REQUIRE_FALSE(therm.is_stable());
}

// -----------------------------------------------------------------------------
// Lookup table
// -----------------------------------------------------------------------------

TEST_CASE("Lookup table computes exact intermediate points") {
  static LookupEntry custom_table[] = {{0.0, 10.0}, {1023.0, 30.0}};

  ThermistorConfig cfg;
  cfg.use_lookup_table = true;
  cfg.lookup_table = custom_table;
  cfg.lookup_size = 2;
  cfg.samples = 1;

  SmoothThermistor therm(0, cfg, adc_mid);

  REQUIRE(therm.tick());
  REQUIRE(therm.temperature() == Catch::Approx(20.0).margin(0.1));
}

// -----------------------------------------------------------------------------
// ADC resolution
// -----------------------------------------------------------------------------

TEST_CASE("Different ADC resolutions produce close but distinct temperatures") {
  ThermistorConfig cfg10;
  cfg10.adc_resolution = AdcResolution::bit10;
  cfg10.samples = 1;

  ThermistorConfig cfg12;
  cfg12.adc_resolution = AdcResolution::bit12;
  cfg12.samples = 1;

  auto adc_const_512 = adc_mid;

  SmoothThermistor t10(0, cfg10, adc_const_512);
  SmoothThermistor t12(0, cfg12, adc_const_512);

  REQUIRE(t10.tick());
  REQUIRE(t12.tick());

  double temp10 = t10.temperature();
  double temp12 = t12.temperature();

  REQUIRE(std::isfinite(temp10));
  REQUIRE(std::isfinite(temp12));
  REQUIRE(temp10 != temp12);
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
  cfg.samples = 1;

  SmoothThermistor therm(0, cfg, adc_mid);

  REQUIRE(therm.tick());
  double t = therm.temperature();
  REQUIRE(t > -50.0);
  REQUIRE(t < 150.0);
}

// =============================================================================
// NEW ADDED TEST CASES (Advanced Engine Verification)
// =============================================================================

TEST_CASE(
    "NEW: Noise estimation evaluates cleanly with absolute zero variance") {
  ThermistorConfig cfg;
  cfg.enable_noise_estimation = true;
  cfg.samples = 1;

  auto zero_jitter_adc = adc_mid; // constant 512
  SmoothThermistor therm(0, cfg, zero_jitter_adc);

  for (int i = 0; i < 20; ++i) {
    therm.tick();
    delay(1);
  }

  REQUIRE(therm.last_noise_stddev() == Catch::Approx(0.0).margin(1e-5));
}

TEST_CASE("NEW: Lookup table interpolation clips securely at array limits") {
  static LookupEntry explicit_limits_table[] = {
      {200.0, -10.0}, {500.0, 25.0}, {800.0, 85.0}};

  ThermistorConfig cfg;
  cfg.use_lookup_table = true;
  cfg.lookup_table = explicit_limits_table;
  cfg.lookup_size = 3;
  cfg.samples = 1;

  SECTION("Below minimum array threshold constraint bounds") {
    auto extreme_low_adc = adc_low;
    SmoothThermistor therm(0, cfg, extreme_low_adc);
    REQUIRE(therm.tick());
    REQUIRE(therm.temperature() == Catch::Approx(-10.0).margin(0.01));
  }

  SECTION("Above maximum array threshold constraint bounds") {
    auto extreme_high_adc = adc_high;
    SmoothThermistor therm(0, cfg, extreme_high_adc);
    REQUIRE(therm.tick());
    REQUIRE(therm.temperature() == Catch::Approx(85.0).margin(0.01));
  }
}

TEST_CASE("NEW: Exponential filter scaling changes response rate parameters") {
  ThermistorConfig cfg_heavy;
  cfg_heavy.filter_mode = FilterMode::exponential;
  cfg_heavy.exp_alpha = 0.1;
  cfg_heavy.samples = 1;

  ThermistorConfig cfg_bypass;
  cfg_bypass.filter_mode = FilterMode::exponential;
  cfg_bypass.exp_alpha = 1.0;
  cfg_bypass.samples = 1;

  g_dynamic_signal = 300;
  SmoothThermistor therm_heavy(0, cfg_heavy, dynamic_adc);
  SmoothThermistor therm_bypass(0, cfg_bypass, dynamic_adc);

  REQUIRE(therm_heavy.tick());
  REQUIRE(therm_bypass.tick());

  g_dynamic_signal = 800;
  delay(1);
  REQUIRE(therm_heavy.tick());
  delay(1);
  REQUIRE(therm_bypass.tick());

  REQUIRE(therm_bypass.temperature() > therm_heavy.temperature());
}

TEST_CASE(
    "NEW: Asynchronous window processing buffers input during clock loops") {
  ThermistorConfig cfg;
  cfg.samples = 4;
  cfg.oversample_factor = 1;

  g_stepped_signal = 500;
  SmoothThermistor therm(0, cfg, stepped_adc);

  REQUIRE_FALSE(therm.tick());
  delay(1);
  REQUIRE_FALSE(therm.tick());
  delay(1);

  g_stepped_signal = 900;
  REQUIRE_FALSE(therm.tick());
  delay(1);

  REQUIRE(therm.tick());
  REQUIRE(therm.temperature() > 0.0);
}
