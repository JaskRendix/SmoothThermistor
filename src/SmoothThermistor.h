/*
 * This file is part of SmoothThermistor.
 *
 * Original work:
 * Copyright (c) 2016 Gianni Van Hoecke <gianni.vh@gmail.com>
 *
 * Modernized version:
 * Copyright (c) 2026 Giorgio
 *
 * Licensed under the MIT License.
 */

#pragma once

#include <cstdint>

namespace smooththermistor {

enum class AdcResolution : std::uint8_t { bit10 = 10, bit12 = 12 };

enum class FilterMode : std::uint8_t {
  none,
  exponential,
  moving_average,
  median
};

struct LookupEntry {
  double adc_value;
  double temperature;
};

struct ThermistorConfig {
  std::uint16_t samples = 1;
  std::uint16_t oversample_factor = 1;

  AdcResolution adc_resolution = AdcResolution::bit10;

  bool use_lookup_table = false;
  const LookupEntry *lookup_table = nullptr;
  std::size_t lookup_size = 0;

  bool use_full_steinhart = false;
  double sh_a = 0.0;
  double sh_b = 0.0;
  double sh_c = 0.0;

  double nominal_resistance = 10000.0;
  double nominal_temperature = 25.0;
  double beta_coefficient = 3950.0;

  FilterMode filter_mode = FilterMode::none;
  double exp_alpha = 0.5;

  bool enable_noise_estimation = false;
  double stability_threshold = 0.5;

  std::uint32_t series_resistance = 10000;
};

using AdcReader = std::uint16_t (*)(std::uint8_t);

class SmoothThermistor {
public:
  static constexpr std::size_t history_size_ = 16;

  explicit SmoothThermistor(std::uint8_t analog_pin,
                            const ThermistorConfig &config,
                            AdcReader adc_reader = nullptr);

  void use_aref(bool enabled);

  bool tick();
  double temperature();

  bool is_stable() const;
  double last_noise_stddev() const;
  double last_noise_variance() const;
  double last_filtered() const;

private:
  double read_average_adc_blocking() const;
  double adc_to_resistance(double adc_value) const;
  double compute_temperature_from_resistance(double R) const;
  double apply_filter(double raw_temp);
  void update_history(double temp);
  void update_noise_stats();
  void update_stability();
  double lookup_temperature(double adc_value) const;

  std::uint8_t analog_pin_;
  ThermistorConfig config_;
  AdcReader adc_reader_;

  bool aref_external_ = false;
  std::uint32_t last_sample_time_ = 0;
  std::uint32_t accumulated_adc_sum_ = 0;
  std::uint32_t sample_counter_ = 0;

  double temperature_history_[history_size_]{};
  std::size_t history_index_ = 0;
  bool history_filled_ = false;

  double last_temperature_ = 0.0;
  double last_filtered_ = 0.0;
  double last_noise_stddev_ = 0.0;
  double last_noise_variance_ = 0.0;
  bool stable_ = false;
};

} // namespace smooththermistor
