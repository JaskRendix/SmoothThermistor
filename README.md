# SmoothThermistor

A thermistor‑reading library using the Beta model or the Steinhart–Hart equation.  
This version modernizes the original library while preserving its basic constructor and behavior.

Original library:  
[https://github.com/giannivh/SmoothThermistor/](https://github.com/giannivh/SmoothThermistor/)

---

# Installation

Place the library folder into `Arduino/libraries/`.  
The public API preserves the original constructor for basic sketches.

---

# Circuit

```
              Analog pin 0
                    |
5V |-----/\/\/\-----+-----/\/\/\-----| GND

           ^                ^ 
    10K thermistor     10K resistor
```

Optional AREF circuit:

```
      AREF      Analog pin 0
       |              |
3.3V |-+---/\/\/\-----+-----/\/\/\-----| GND

             ^                ^ 
      10K thermistor     10K resistor
```

---

# Usage

Include the library:

```cpp
#include <SmoothThermistor.h>
```

Basic constructor (namespace required):

```cpp
smooththermistor::SmoothThermistor therm(A0);
```

Configuration‑based constructor:

```cpp
smooththermistor::ThermistorConfig cfg;
cfg.adc_resolution = smooththermistor::AdcResolution::bit12;
cfg.nominal_resistance = 10000;
cfg.series_resistance = 10000;
cfg.beta_coefficient = 3950;
cfg.samples = 16;
cfg.filter_mode = smooththermistor::FilterMode::moving_average;

smooththermistor::SmoothThermistor therm(A0, cfg);
```

Optional AREF scaling:

```cpp
therm.use_aref(true);
```

Read temperature:

```cpp
double t = therm.temperature();
```

---

# API Overview

## Core Types

| Type | Description |
|------|-------------|
| smooththermistor::SmoothThermistor | Main thermistor reader |
| smooththermistor::ThermistorConfig | Configuration struct |
| smooththermistor::AdcResolution | ADC resolution enum |
| smooththermistor::FilterMode | Filtering mode enum |

---

## ThermistorConfig Fields

| Field | Type | Meaning |
|-------|------|---------|
| adc_resolution | AdcResolution | ADC bit depth |
| samples | std::size_t | Number of ADC samples |
| oversample_factor | std::size_t | Additional averaging |
| nominal_resistance | double | Thermistor nominal value |
| series_resistance | double | Series resistor |
| beta_coefficient | double | Beta constant |
| use_full_steinhart | bool | Enable A/B/C coefficients |
| sh_a, sh_b, sh_c | double | Steinhart–Hart coefficients |
| filter_mode | FilterMode | Filtering method |
| enable_noise_estimation | bool | Track noise variance |
| stability_threshold | double | Stability detection threshold |

---

## SmoothThermistor Methods

| Method | Returns | Description |
|--------|---------|-------------|
| temperature() | double | Current filtered temperature |
| tick() | bool | Non‑blocking update step |
| last_filtered() | double | Last filtered temperature |
| is_stable() | bool | Stability flag |
| last_noise_stddev() | double | Noise standard deviation |
| last_noise_variance() | double | Noise variance |
| use_aref(bool) | void | Enable AREF scaling |

---

# Non‑Blocking Engine

The library supports a non‑blocking mode using `tick()`:

```cpp
if (therm.tick()) {
    double t = therm.last_filtered();
}
```

`tick()` performs:

- oversampling  
- ADC accumulation  
- resistance calculation  
- filtering  
- noise estimation (optional)  
- stability detection (optional)

without blocking the main loop.

The synchronous `temperature()` method remains available for drop‑in compatibility.

---

# Filtering Modes

### Moving average  
Sliding window average over recent raw temperature values.

### Median  
Sorts the history window and selects the median. Useful for rejecting spikes.

### Exponential  
Weighted filter using `exp_alpha`. Faster response with less smoothing.

---

# Steinhart–Hart Mode

```cpp
cfg.use_full_steinhart = true;
cfg.sh_a = 1.009249522e-03;
cfg.sh_b = 2.378405444e-04;
cfg.sh_c = 2.019202697e-07;
```

---

# Example Snippets

### Moving average + noise estimation

```cpp
smooththermistor::ThermistorConfig cfg;
cfg.filter_mode = smooththermistor::FilterMode::moving_average;
cfg.samples = 20;
cfg.enable_noise_estimation = true;

smooththermistor::SmoothThermistor therm(A0, cfg);

void loop() {
    Serial.println(therm.temperature());
    Serial.println(therm.last_noise_stddev());
}
```

### Steinhart–Hart mode

```cpp
smooththermistor::ThermistorConfig cfg;
cfg.use_full_steinhart = true;
cfg.sh_a = 1.009249522e-03;
cfg.sh_b = 2.378405444e-04;
cfg.sh_c = 2.019202697e-07;

smooththermistor::SmoothThermistor therm(A0, cfg);
```

### Custom ADC reader

```cpp
cfg.adc_reader = []() {
    return analogRead(A0);
};
```

---

# Filtering Pipeline

```
ADC read
↓
Oversampling (optional)
↓
Resistance calculation (Beta or Steinhart–Hart)
↓
Filtering (moving average / median / exponential)
↓
Noise estimation (optional)
↓
Stability detection (optional)
↓
Temperature output
```

---

# Compatibility

The original constructor remains:

```cpp
smooththermistor::SmoothThermistor therm(A0);
```

Existing sketches only need to add the namespace.

---

# ADC Resolution

```
smooththermistor::AdcResolution::bit8
smooththermistor::AdcResolution::bit10
smooththermistor::AdcResolution::bit12
smooththermistor::AdcResolution::bit16
```

Default: 10‑bit.
