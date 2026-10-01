# Drone Tilt Detection

**Attitude estimation and a tilt alarm for a drone, on an ESP32-S3.** Two sensors that are each
unusable on their own, fused into one angle estimate that is smooth *and* still correct a minute
later.

Built as the final project of a six-week summer technical training programme at India Space Lab,
2025. Developed and wired in [Wokwi](https://wokwi.com), a browser-based hardware simulator.

`C++` · `ESP32-S3` · `MPU6050` · `Sensor Fusion` · `Keras` · `TensorFlow Lite`

---

## The problem

A drone that tips too far starts losing lift on the low side. Past a certain angle it stops being a
tilt and becomes a crash. So: work out the drone's angle from its IMU, and warn before it gets
there.

That sounds trivial until you try to read the angle, because the MPU6050 contains two instruments
and **each is bad in exactly the way the other is not**:

| Sensor | Good | Bad |
|---|---|---|
| **Accelerometer** | Senses gravity, so it gives an *absolute* reference that never slowly goes wrong | Senses *all* acceleration — every speed-up, turn or gust reads as tilt that isn't there |
| **Gyroscope** | Senses rotation rate; integrate it and you get angle. Smooth, and completely immune to turbulence | Every reading carries a small error, and integrating error means the total slides away without bound |

One is correct on average but jumpy. The other is steady but drifts. Neither works alone.

## The approach — a complementary filter

Each sensor is strong precisely where the other is weak, so blend them:

```
roll  = ALPHA * (roll  + gyroRollRate  * dt) + (1 - ALPHA) * accRoll;
pitch = ALPHA * (pitch + gyroPitchRate * dt) + (1 - ALPHA) * accPitch;
```

with `ALPHA = 0.96f`.

Read it as: **mostly trust the gyroscope moment to moment, but nudge 4% of the way toward gravity
every cycle.** The gyroscope supplies smooth, fast response; the constant small pull toward the
accelerometer's absolute reference stops drift ever accumulating. The result is stable *and* stays
true over time, from two sensors that cannot do that individually.

`ALPHA` is the tuning knob. Higher is smoother but drifts more; lower kills drift but shakes with
every bump. 0.96 was chosen for this vehicle.

**The timestep is measured, not assumed:**

```c
uint32_t now = micros();
float dt = (now - lastMicros) * 1e-6f;
```

The loop targets 100 Hz via `delay(10)`, but the filter integrates against the *actual* elapsed
time rather than a hard-coded 10 ms. Assuming a fixed loop rate is the usual way this goes quietly
wrong when the I2C read or the display update takes longer than expected.

Past `TILT_THRESHOLD` (45°) the firmware lights the red LED, sounds the buzzer, and writes
`DANGER!` / `Tilt > 45 deg` to the OLED. Otherwise: green LED, `Status: Normal`.

---

## What runs where

This repo contains two pieces of work. **They are not yet one pipeline, and it's worth being
explicit about that:**

| | Status |
|---|---|
| **Complementary filter + threshold alarm** | Runs on the device. This is what `sketch.ino` actually executes. |
| **Neural-network classifier** | Trained, exported to TensorFlow Lite, and converted to a C byte array ready for embedding — but **`sketch.ino` does not yet `#include "model_data.h"`**, so it is not run at inference time. |

Wiring the classifier into the firmware is the next piece of work (see [Status](#status)). Until it
is, the threshold is what decides the danger state — and for this particular task the threshold
works fine, which is itself a reasonable finding.

---

## Hardware

| Part | Connection | Role |
|---|---|---|
| ESP32-S3 DevKitC-1 | — | Runs the firmware |
| MPU6050 | I2C — GPIO 8 (SDA) / GPIO 9 (SCL) | 3-axis accelerometer + 3-axis gyroscope |
| SSD1306 OLED 128×64 | same I2C bus, address `0x3C` | Live tilt readout and status |
| Green LED | GPIO 4, 220 Ω | All clear |
| Red LED | GPIO 5, 220 Ω | Danger |
| Buzzer | GPIO 6 | 1 kHz alarm |

`diagram.json` is the Wokwi wiring for exactly this layout.

## Running it

No physical hardware needed:

1. Open [wokwi.com](https://wokwi.com) and start a new **ESP32** project
2. Paste `sketch.ino` into the code tab and `diagram.json` into the diagram tab
3. Run, then drag the MPU6050 past 45° with the mouse
4. The OLED switches to `DANGER!`, the red LED lights and the buzzer sounds

---

## The machine-learning half

`train_model.py` trains a small MLP to classify normal flight against dangerous tilt directly from
raw IMU values.

```
Input    6 features — ax, ay, az, gx, gy, gz
         ↓  Dense(8, relu)
         ↓  Dense(4, relu)
         ↓  Dense(2, softmax)
Output   normal | tilt
```

- Preprocessing: `StandardScaler()`
- Optimizer `adam`, loss `sparse_categorical_crossentropy`, 30 epochs, batch size 16
- Exported with `tf.lite.TFLiteConverter.from_keras_model()`, no quantization
- **`model_tflite_len = 2512`** — the converted model is **2.5 KB**, small enough to sit in flash
  alongside the firmware with room to spare

`convert_to_header.py` turns `model.tflite` into `model_data.h`, emitting `model_tflite[]` and
`model_tflite_len` as a C hex array for embedded use.

> The header file itself is ~15 KB on disk because each byte becomes six characters of hex text.
> The model is 2.5 KB.

---

## Status

Honest list of what is not done. These are the next commits, not excuses.

- [ ] **Wire the classifier into the firmware.** Add TensorFlow Lite Micro, `#include "model_data.h"`,
      set up an interpreter with a ~4 KB tensor arena, and drive the danger state from the softmax
      output. Keep the threshold as a fallback and show both on the OLED — `THRESH: OK | NN: 0.87`
      is a better demo than either alone.
- [ ] **Persist the scaler.** `train_model.py` prints `scaler.mean_` and `scaler.scale_` but does not
      save them. They must be hard-coded into the sketch before inference runs, or the model will
      silently produce nonsense because inference inputs sit on a different scale from training
      inputs. This is the classic TinyML failure and it fails quietly.
- [ ] **Commit `normal.csv` and `tilt.csv`.** The training script reads them and they are not in the
      repo, so the result is currently not reproducible by anyone, including me.
- [ ] **Hold out a test set.** Training currently fits on the entire dataset with no split, so there
      is no honest accuracy figure to report — and so none is reported here.

## Repository contents

| File | Purpose |
|---|---|
| `sketch.ino` | ESP32-S3 firmware — I2C, complementary filter, OLED, LEDs, buzzer |
| `diagram.json` | Wokwi wiring for the circuit above |
| `train_model.py` | Keras MLP on MPU6050 data, exports TFLite |
| `convert_to_header.py` | TFLite binary → C byte array |
| `model_data.h` | The 2.5 KB embedded model, ready to include |

## License

See [LICENSE](LICENSE).
