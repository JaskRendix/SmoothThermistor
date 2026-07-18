/*
 * This file is part of SmoothThermistor.
 *
 * Original work:
 * Copyright (c) 2016 Gianni Van Hoecke <gianni.vh@gmail.com>
 *
 * Modernized version:
 * Copyright (c) 2026 Giorgio <your-email-here>
 *
 * Licensed under the MIT License.
 */

#ifndef SMOOTHTHERMISTOR_H
#define SMOOTHTHERMISTOR_H

#include "Arduino.h"
#include <cstdint>
#include <functional>

namespace smooththermistor {

// Strongly typed ADC resolution
enum class AdcResolution : std::uint16_t {
  bit8 = 8,
  bit10 = 10,
  bit12 = 12,
  bit16 = 16
};

// Filtering modes
enum class FilterMode : std::uint8_t {
  none,
  moving_average,
  median,
  exponential
};

// Default configuration constants
constexpr std::uint32_t default_nominal_resistance = 10'000;
constexpr std::uint32_t default_series_resistance = 10'000;
constexpr std::uint16_t default_beta_coefficient = 3950;
constexpr std::uint8_t default_nominal_temperature = 25;
constexpr std::uint8_t default_samples = 10;
constexpr std::uint8_t default_oversample_factor = 1;
constexpr double default_exp_alpha = 0.1;
constexpr double default_stability_threshold = 0.05; // °C

// Hardware abstraction: ADC reader callback
using AdcReader = std::function<std::uint16_t(std::uint8_t)>;

// Configuration struct (modern replacement for multi‑arg constructor)
struct ThermistorConfig {
  // Core thermistor parameters
  AdcResolution adc_resolution{AdcResolution::bit10};
  std::uint32_t nominal_resistance{default_nominal_resistance};
  std::uint32_t series_resistance{default_series_resistance};
  std::uint16_t beta_coefficient{default_beta_coefficient};
  std::uint8_t nominal_temperature{default_nominal_temperature};

  // Sampling / smoothing
  std::uint8_t samples{default_samples};
  std::uint8_t oversample_factor{default_oversample_factor};
  FilterMode filter_mode{FilterMode::none};
  double exp_alpha{default_exp_alpha};

  // Full Steinhart–Hart coefficients (optional)
  bool use_full_steinhart{false};
  double sh_a{0.0};
  double sh_b{0.0};
  double sh_c{0.0};

  // Noise estimation / stability
  bool enable_noise_estimation{false};
  double stability_threshold{default_stability_threshold};

  // Lookup table mode (optional)
  bool use_lookup_table{false};
  const double *lookup_table{nullptr};
  std::size_t lookup_size{0};
};

class SmoothThermistor {
public:
  /**
   * @param analog_pin   The analog pin where the thermistor is connected.
   * @param config       Thermistor configuration (ADC, resistances, beta,
   * filters, etc.).
   * @param adc_reader   Callback used to read the ADC value. Defaults to
   * ::analogRead.
   */
  explicit SmoothThermistor(std::uint8_t analog_pin,
                            ThermistorConfig config = ThermistorConfig{},
                            AdcReader adc_reader = AdcReader{::analogRead});

  // Enable or disable AREF usage (EXTERNAL vs DEFAULT)
  void use_aref(bool enabled);

  // Get the smoothed temperature in degrees Celsius
  double temperature();

  // Check if the temperature is considered stable (based on recent history)
  bool is_stable() const;

  // Last estimated noise statistics (if enabled)
  double last_noise_stddev() const;
  double last_noise_variance() const;

private:
  // Core
  std::uint8_t analog_pin_{};
  ThermistorConfig config_{};
  AdcReader adc_reader_{};
  bool aref_external_{false};

  // Internal state for filtering / stability / noise
  static constexpr std::size_t history_size_ = 16;
  double temperature_history_[history_size_]{};
  std::size_t history_index_{0};
  bool history_filled_{false};

  double last_temperature_{0.0};
  double last_filtered_{0.0};
  double last_noise_stddev_{0.0};
  double last_noise_variance_{0.0};
  bool stable_{false};

  // Internal helpers (implemented in .cpp)
  double read_average_adc() const;
  double adc_to_resistance(double adc_value) const;
  double compute_temperature_from_resistance(double resistance) const;
  double apply_filter(double raw_temp);
  void update_history(double temp);
  void update_noise_stats();
  void update_stability();
  double lookup_temperature(double resistance) const;
};

} // namespace smooththermistor

#endif // SMOOTHTHERMISTOR_H
