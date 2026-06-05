# Calibration

The conversion from colour ratio to temperature has two parameters:

```
T = B / ln(q / K)        q = fR / fG   (RED / GREEN frequency ratio)
```

- **B** (slope) comes from the channel wavelengths and is set from
  `PYRO_LAMBDA_R_NM` / `PYRO_LAMBDA_G_NM` in
  [`src/pyrometer.h`](../src/pyrometer.h).
- **K** depends on the *relative responsivity* of the two channels (and your
  optics/IR filter). It is large and unknowable up front, so **you must
  calibrate**. Until you do, the firmware prints `[UNCALIBRATED - relative
  only]` and the numbers are not real degrees.

You need a reference temperature: a thermocouple/IR thermometer on the same
glowing object, a temperature-controlled tube furnace, or a lamp with a known
colour temperature.

## One-point calibration (minimum)

1. Build and flash with no calibration defined; open the serial terminal.
2. Heat the target to a known steady temperature `T_ref` (°C). Read it with
   your reference instrument.
3. Aim the sensor and note the printed `q=` value (average a few readings).
4. In [`src/main.c`](../src/main.c) set:
   ```c
   #define PYRO_ONE_POINT_TC  1000.0   /* your T_ref in °C        */
   #define PYRO_ONE_POINT_Q   0.620    /* the q you measured      */
   ```
5. Rebuild and flash. The `[UNCALIBRATED]` tag disappears and `T=` should now
   read close to `T_ref` near that point.

One point pins the curve at a single temperature; accuracy degrades as you move
away from it.

## Two-point calibration (better)

Take `q` at two known temperatures `T1`, `T2` (spread them across your range).
Replace the one-point block in `main()` with:

```c
pyro_cal_two_point(&cal,
                   pyro_C2K(T1_C), q1,
                   pyro_C2K(T2_C), q2);
bool calibrated = true;
```

This fits both `B` and `K` to your actual sensor + optics, which is more
accurate than the wavelength-derived slope.

## Tips & honest limits

- The TCS3200's filters are broad and overlapping, so this is an
  *approximate* pyrometer. Expect best results within the calibrated range.
- Re-check calibration if you change distance optics, add/remove an IR filter,
  or change the sensor's power supply voltage.
- If `q` doesn't change monotonically with temperature, your target may be too
  cool (mostly IR), saturating, or contaminated by reflected/background light.
- For dim or low-temperature glow, increase the OUT scaling
  (`tcs3200_set_scale(TCS_SCALE_20)`) and/or the gate time (`GATE_MS`) to get
  more edges per measurement.
