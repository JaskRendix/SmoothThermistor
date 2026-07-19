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

/*
 * This file is part of SmoothThermistor.
 * Licensed under the MIT License.
 */

#include "SmoothThermistor.h"
#include "Arduino.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace smooththermistor {

SmoothThermistor::SmoothThermistor(std::uint8_t analog_pin,
                                   const ThermistorConfig &config,
                                   AdcReader adc_reader)
    : analog_pin_{analog_pin}, config_{config},
      adc_reader_{adc_reader ? adc_reader : ::analogRead} {
  std::fill(temperature_history_, temperature_history_ + history_size_, 0.0);
}

void SmoothThermistor::use_aref(bool enabled) {
  aref_external_ = enabled;
#if !defined(ESP32) && !defined(ESP8266)
  analogReference(enabled ? EXTERNAL : DEFAULT);
#endif
}

/* -------------------------------------------------------------------------
 * NON‑BLOCKING ENGINE
 * ------------------------------------------------------------------------- */
bool SmoothThermistor::tick() {
  const std::uint32_t now = millis();
  const std::uint32_t total_needed =
      static_cast<std::uint32_t>(config_.samples) *
      static_cast<std::uint32_t>(config_.oversample_factor);

  if (total_needed == 0)
    return false;

  if (now - last_sample_time_ >= 1) {
    last_sample_time_ = now;

    accumulated_adc_sum_ += adc_reader_(analog_pin_);
    sample_counter_++;

    if (sample_counter_ >= total_needed) {
      const double avg_adc = static_cast<double>(accumulated_adc_sum_) /
                             static_cast<double>(total_needed);

      accumulated_adc_sum_ = 0;
      sample_counter_ = 0;

      double temp = 0.0;

      if (config_.use_lookup_table && config_.lookup_table &&
          config_.lookup_size > 0) {
        temp = lookup_temperature(avg_adc);
      } else {
        const double resistance = adc_to_resistance(avg_adc);
        temp = compute_temperature_from_resistance(resistance);
      }

      update_history(temp);
      last_filtered_ = apply_filter(temp);
      last_temperature_ = last_filtered_;

      if (config_.enable_noise_estimation)
        update_noise_stats();

      update_stability();
      return true;
    }
  }
  return false;
}

/* -------------------------------------------------------------------------
 * SYNCHRONOUS DROP‑IN MODE
 * ------------------------------------------------------------------------- */
double SmoothThermistor::temperature() {
  const double avg_adc = read_average_adc_blocking();

  double temp = 0.0;

  if (config_.use_lookup_table && config_.lookup_table &&
      config_.lookup_size > 0) {
    temp = lookup_temperature(avg_adc);
  } else {
    const double resistance = adc_to_resistance(avg_adc);
    temp = compute_temperature_from_resistance(resistance);
  }

  update_history(temp);
  last_filtered_ = apply_filter(temp);
  last_temperature_ = last_filtered_;

  if (config_.enable_noise_estimation)
    update_noise_stats();

  update_stability();

  return last_temperature_;
}

/* -------------------------------------------------------------------------
 * GETTERS
 * ------------------------------------------------------------------------- */
bool SmoothThermistor::is_stable() const { return stable_; }
double SmoothThermistor::last_noise_stddev() const {
  return last_noise_stddev_;
}
double SmoothThermistor::last_noise_variance() const {
  return last_noise_variance_;
}
double SmoothThermistor::last_filtered() const { return last_filtered_; }

/* -------------------------------------------------------------------------
 * BLOCKING ADC AVERAGE
 * ------------------------------------------------------------------------- */
double SmoothThermistor::read_average_adc_blocking() const {
  const std::uint32_t total =
      static_cast<std::uint32_t>(config_.samples) *
      static_cast<std::uint32_t>(config_.oversample_factor);

  double sum = 0.0;

  for (std::uint32_t i = 0; i < total; ++i) {
    sum += static_cast<double>(adc_reader_(analog_pin_));
    delay(1);
  }

  return sum / static_cast<double>(total);
}

/* -------------------------------------------------------------------------
 * ADC → RESISTANCE
 * ------------------------------------------------------------------------- */
double SmoothThermistor::adc_to_resistance(double adc_value) const {
  const auto bits = static_cast<unsigned>(config_.adc_resolution);
  const double max_adc = static_cast<double>((1UL << bits) - 1UL);

  if (adc_value <= 0.0)
    return static_cast<double>(config_.series_resistance) * 1e6;

  return static_cast<double>(config_.series_resistance) *
         (max_adc / adc_value - 1.0);
}

/* -------------------------------------------------------------------------
 * RESISTANCE → TEMPERATURE
 * ------------------------------------------------------------------------- */
double SmoothThermistor::compute_temperature_from_resistance(double R) const {
  if (config_.use_full_steinhart) {
    const double lnR = std::log(R);
    const double invT =
        config_.sh_a + config_.sh_b * lnR + config_.sh_c * std::pow(lnR, 3.0);

    return (1.0 / invT) - 273.15;
  }

  const double ln_ratio =
      std::log(R / static_cast<double>(config_.nominal_resistance));

  const double inv_T0 =
      1.0 / (static_cast<double>(config_.nominal_temperature) + 273.15);

  double steinhart =
      ln_ratio / static_cast<double>(config_.beta_coefficient) + inv_T0;

  return (1.0 / steinhart) - 273.15;
}

/* -------------------------------------------------------------------------
 * FILTERING
 * ------------------------------------------------------------------------- */
double SmoothThermistor::apply_filter(double raw_temp) {
  const std::size_t count = history_filled_ ? history_size_ : history_index_;

  if (count == 0) {
    last_filtered_ = raw_temp;
    return raw_temp;
  }

  switch (config_.filter_mode) {
  case FilterMode::exponential:
    last_filtered_ = config_.exp_alpha * raw_temp +
                     (1.0 - config_.exp_alpha) * last_filtered_;
    return last_filtered_;

  case FilterMode::moving_average: {
    const double sum = std::accumulate(temperature_history_,
                                       temperature_history_ + count, 0.0);
    last_filtered_ = sum / static_cast<double>(count);
    return last_filtered_;
  }

  case FilterMode::median: {
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

/* -------------------------------------------------------------------------
 * HISTORY
 * ------------------------------------------------------------------------- */
void SmoothThermistor::update_history(double temp) {
  temperature_history_[history_index_] = temp;
  history_index_ = (history_index_ + 1) % history_size_;

  if (history_index_ == 0)
    history_filled_ = true;
}

/* -------------------------------------------------------------------------
 * NOISE
 * ------------------------------------------------------------------------- */
void SmoothThermistor::update_noise_stats() {
  const std::size_t count = history_filled_ ? history_size_ : history_index_;

  if (count < 2) {
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

/* -------------------------------------------------------------------------
 * STABILITY
 * ------------------------------------------------------------------------- */
void SmoothThermistor::update_stability() {
  const std::size_t count = history_filled_ ? history_size_ : history_index_;

  if (count < 2) {
    stable_ = false;
    return;
  }

  const std::size_t latest =
      (history_index_ + history_size_ - 1) % history_size_;

  const std::size_t prev = (history_index_ + history_size_ - 2) % history_size_;

  const double delta =
      std::abs(temperature_history_[latest] - temperature_history_[prev]);

  stable_ = (delta < config_.stability_threshold);
}

/* -------------------------------------------------------------------------
 * LOOKUP TABLE (ADC → temperature)
 * ------------------------------------------------------------------------- */
double SmoothThermistor::lookup_temperature(double adc_value) const {
  auto cmp = [](const LookupEntry &e, double v) { return e.adc_value < v; };

  const LookupEntry *begin = config_.lookup_table;
  const LookupEntry *end = config_.lookup_table + config_.lookup_size;

  const LookupEntry *it = std::lower_bound(begin, end, adc_value, cmp);

  if (it == begin)
    return begin->temperature;

  if (it == end)
    return (end - 1)->temperature;

  const LookupEntry *prev = it - 1;

  const double frac =
      (adc_value - prev->adc_value) / (it->adc_value - prev->adc_value);

  return prev->temperature + frac * (it->temperature - prev->temperature);
}

} // namespace smooththermistor
