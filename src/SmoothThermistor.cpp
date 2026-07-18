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

#include "SmoothThermistor.h"
#include "Arduino.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace smooththermistor {

SmoothThermistor::SmoothThermistor(std::uint8_t analog_pin,
                                   ThermistorConfig config,
                                   AdcReader adc_reader)
    : analog_pin_{analog_pin}, config_{config},
      adc_reader_{std::move(adc_reader)}, aref_external_{false},
      history_index_{0}, history_filled_{false}, last_temperature_{0.0},
      last_filtered_{0.0}, last_noise_stddev_{0.0}, last_noise_variance_{0.0},
      stable_{false} {
  std::fill(std::begin(temperature_history_), std::end(temperature_history_),
            0.0);
}

void SmoothThermistor::use_aref(bool enabled) {
  aref_external_ = enabled;
#if !defined(ESP32) && !defined(ESP8266)
  analogReference(enabled ? EXTERNAL : DEFAULT);
#endif
}

double SmoothThermistor::temperature() {
  const double avg_adc = read_average_adc();
  const double resistance = adc_to_resistance(avg_adc);
  double temp = compute_temperature_from_resistance(resistance);

  temp = apply_filter(temp);
  last_temperature_ = temp;

  update_history(temp);

  if (config_.enable_noise_estimation) {
    update_noise_stats();
  }

  update_stability();

  return temp;
}

bool SmoothThermistor::is_stable() const { return stable_; }

double SmoothThermistor::last_noise_stddev() const {
  return last_noise_stddev_;
}

double SmoothThermistor::last_noise_variance() const {
  return last_noise_variance_;
}

// -----------------------------
// Internal helpers
// -----------------------------

double SmoothThermistor::read_average_adc() const {
  const std::uint32_t total_samples =
      static_cast<std::uint32_t>(config_.samples) *
      static_cast<std::uint32_t>(config_.oversample_factor);

  double sum = 0.0;

  for (std::uint32_t i = 0; i < total_samples; ++i) {
    sum += static_cast<double>(adc_reader_(analog_pin_));
    delay(1);
  }

  return sum / static_cast<double>(total_samples);
}

double SmoothThermistor::adc_to_resistance(double adc_value) const {
  const auto bits = static_cast<unsigned>(config_.adc_resolution);
  const double max_adc = static_cast<double>((1u << bits) - 1u);

  if (adc_value <= 0.0) {
    return static_cast<double>(config_.series_resistance) * 1e6;
  }

  const double ratio = max_adc / adc_value - 1.0;
  return static_cast<double>(config_.series_resistance) * ratio;
}

double
SmoothThermistor::compute_temperature_from_resistance(double resistance) const {
  if (config_.use_lookup_table && config_.lookup_table &&
      config_.lookup_size > 0) {
    return lookup_temperature(resistance);
  }

  if (config_.use_full_steinhart) {
    const double lnR = std::log(resistance);
    const double invT =
        config_.sh_a + config_.sh_b * lnR + config_.sh_c * std::pow(lnR, 3.0);
    const double tempK = 1.0 / invT;
    return tempK - 273.15;
  }

  const double ln_ratio =
      std::log(resistance / static_cast<double>(config_.nominal_resistance));
  const double inv_T0 =
      1.0 / (static_cast<double>(config_.nominal_temperature) + 273.15);

  double steinhart =
      ln_ratio / static_cast<double>(config_.beta_coefficient) + inv_T0;
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;

  return steinhart;
}

double SmoothThermistor::apply_filter(double raw_temp) {
  switch (config_.filter_mode) {
  case FilterMode::none:
    last_filtered_ = raw_temp;
    return raw_temp;

  case FilterMode::exponential: {
    const double alpha = config_.exp_alpha;
    last_filtered_ = alpha * raw_temp + (1.0 - alpha) * last_filtered_;
    return last_filtered_;
  }

  case FilterMode::moving_average: {
    if (!history_filled_ && history_index_ == 0) {
      last_filtered_ = raw_temp;
      return raw_temp;
    }

    const std::size_t count = history_filled_ ? history_size_ : history_index_;
    const double sum = std::accumulate(temperature_history_,
                                       temperature_history_ + count, 0.0);
    last_filtered_ = sum / static_cast<double>(count);
    return last_filtered_;
  }

  case FilterMode::median: {
    if (!history_filled_ && history_index_ == 0) {
      last_filtered_ = raw_temp;
      return raw_temp;
    }

    const std::size_t count = history_filled_ ? history_size_ : history_index_;
    double buffer[history_size_];
    std::copy(temperature_history_, temperature_history_ + count, buffer);

    std::sort(buffer, buffer + count);
    const std::size_t mid = count / 2;
    last_filtered_ =
        (count % 2 == 0) ? 0.5 * (buffer[mid - 1] + buffer[mid]) : buffer[mid];
    return last_filtered_;
  }

  default:
    last_filtered_ = raw_temp;
    return raw_temp;
  }
}

void SmoothThermistor::update_history(double temp) {
  temperature_history_[history_index_] = temp;
  history_index_ = (history_index_ + 1) % history_size_;

  if (history_index_ == 0) {
    history_filled_ = true;
  }
}

void SmoothThermistor::update_noise_stats() {
  const std::size_t count = history_filled_ ? history_size_ : history_index_;
  if (count == 0) {
    last_noise_stddev_ = 0.0;
    last_noise_variance_ = 0.0;
    return;
  }

  const double mean =
      std::accumulate(temperature_history_, temperature_history_ + count, 0.0) /
      static_cast<double>(count);

  double var_sum = 0.0;
  for (std::size_t i = 0; i < count; ++i) {
    const double diff = temperature_history_[i] - mean;
    var_sum += diff * diff;
  }

  last_noise_variance_ = var_sum / static_cast<double>(count);
  last_noise_stddev_ = std::sqrt(last_noise_variance_);
}

void SmoothThermistor::update_stability() {
  const std::size_t count = history_filled_ ? history_size_ : history_index_;
  if (count < 2) {
    stable_ = false;
    return;
  }

  const double latest =
      temperature_history_[(history_index_ + history_size_ - 1) %
                           history_size_];
  const double previous =
      temperature_history_[(history_index_ + history_size_ - 2) %
                           history_size_];

  const double delta = std::abs(latest - previous);
  stable_ = (delta < config_.stability_threshold);
}

double SmoothThermistor::lookup_temperature(double resistance) const {
  if (!config_.lookup_table || config_.lookup_size == 0) {
    return last_filtered_;
  }

  const double idx =
      std::clamp(resistance / static_cast<double>(config_.series_resistance),
                 0.0, static_cast<double>(config_.lookup_size - 1));

  const std::size_t i0 = static_cast<std::size_t>(std::floor(idx));
  const std::size_t i1 = std::min(i0 + 1, config_.lookup_size - 1);

  const double t0 = config_.lookup_table[i0];
  const double t1 = config_.lookup_table[i1];

  const double frac = idx - static_cast<double>(i0);
  return t0 + frac * (t1 - t0);
}

} // namespace smooththermistor
