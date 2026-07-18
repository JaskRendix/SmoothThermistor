# SmoothThermistor

A thermistor‑reading library using the Beta model or the Steinhart–Hart equation.  
This version modernizes the original library while preserving its basic constructor and behavior.

Original library:  
**[https://github.com/giannivh/SmoothThermistor/](https://github.com/giannivh/SmoothThermistor/)**

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

AREF circuit (optional):

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

### Core Types

| Type | Description |
|------|-------------|
| **smooththermistor::SmoothThermistor** | Main thermistor reader |
| **smooththermistor::ThermistorConfig** | Configuration struct |
| **smooththermistor::AdcResolution** | ADC resolution enum |
| **smooththermistor::FilterMode** | Filtering mode enum |

---

### ThermistorConfig Fields

| Field | Type | Meaning |
|-------|------|---------|
| **adc_resolution** | AdcResolution | ADC bit depth |
| **samples** | std::size_t | Number of ADC samples |
| **oversample_factor** | std::size_t | Extra averaging |
| **nominal_resistance** | double | Thermistor nominal value |
| **series_resistance** | double | Series resistor |
| **beta_coefficient** | double | Beta constant |
| **use_full_steinhart** | bool | Enable A/B/C coefficients |
| **sh_a, sh_b, sh_c** | double | Steinhart–Hart coefficients |
| **filter_mode** | FilterMode | Filtering method |
| **enable_noise_estimation** | bool | Track noise variance |
| **stability_threshold** | double | Stability detection |

---

### SmoothThermistor Methods

| Method | Returns | Description |
|--------|---------|-------------|
| **temperature()** | double | Current filtered temperature |
| **raw_adc()** | std::uint32_t | Last raw ADC reading |
| **resistance()** | double | Computed thermistor resistance |
| **is_stable()** | bool | Stability flag |
| **last_noise_stddev()** | double | Noise standard deviation |
| **use_aref(bool)** | void | Enable AREF scaling |

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

# Filtering Pipeline Diagram

A direct representation of the internal processing path:

```
        ┌──────────────┐
        │   ADC read    │  (raw_adc)
        └───────┬──────┘
                ▼
        ┌──────────────┐
        │ Oversampling  │  (optional)
        └───────┬──────┘
                ▼
        ┌──────────────┐
        │  Resistance   │  (Beta or SH)
        └───────┬──────┘
                ▼
        ┌──────────────┐
        │   Filtering   │  (MA / median / exp)
        └───────┬──────┘
                ▼
        ┌──────────────┐
        │ Noise estimate│  (optional)
        └───────┬──────┘
                ▼
        ┌──────────────┐
        │  Stability    │  (optional)
        └───────┬──────┘
                ▼
        ┌──────────────┐
        │  Temperature  │
        └──────────────┘
```

---

# Compatibility

The original constructor remains available:

```cpp
smooththermistor::SmoothThermistor therm(A0);
```

Existing sketches only need to add the namespace.

---

# ADC Resolution

```cpp
smooththermistor::AdcResolution::bit8
smooththermistor::AdcResolution::bit10
smooththermistor::AdcResolution::bit12
smooththermistor::AdcResolution::bit16
```

Default: 10‑bit.
