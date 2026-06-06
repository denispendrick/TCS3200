# TCS3200 pyrometer (dsPIC33)

Measure the temperature of a **glowing-hot object** with a TCS3200
colour-to-frequency sensor and a dsPIC33 microcontroller, by two-colour
(ratio) pyrometry. Results stream over UART.

> **Full write-up** — principle, configuration, circuit diagram, algorithm and
> the complete program — is in **[docs/REPORT.md](docs/REPORT.md)**.
>
> Mounting the sensor for hot work and shielding it from the heat:
> **[docs/sensor-mounting.md](docs/sensor-mounting.md)**.

## Important: the TCS3200 does not sense temperature

The TCS3200 is a **light-to-frequency converter** — it outputs a square wave
whose frequency is proportional to light intensity through Red / Green / Blue /
Clear filters. It has no temperature sensor.

This project infers temperature *indirectly* from **colour**: a hot object
glows dull-red → orange → yellow → white as it heats (blackbody radiation). The
ratio of the RED and GREEN channel frequencies is a monotonic function of
temperature and is largely independent of distance and emissivity — the
principle of two-colour pyrometry.

```
        TCS3200                         dsPIC33
   ┌───────────────┐   OUT (Hz)    ┌──────────────────┐   UART
   │ R/G/B/Clear   ├──────────────►│ Input Capture +  ├────────► PC / terminal
   │ light→freq    │   S0..S3      │ gate count       │  R=..G=..B=..  T=..C
   │ (LEDs OFF)    │◄──────────────┤ colour/scale     │
   └───────┬───────┘   GPIO        └──────────────────┘
           │ sees the glow
        ╲╲╲╲╲╲   hot, glowing object (~600 °C and up)
```

**Works for:** anything that visibly glows — molten metal, kilns, furnaces,
glowing filaments (~600 °C and up).
**Does not work for:** room/body temperatures or anything not glowing. For that
you'd need a different method (e.g. thermochromic material read by the same
sensor, or a real thermal sensor).

## How it works

1. The dsPIC drives **S2/S3** to select each colour filter and **S0/S1** to set
   the output-frequency scaling.
2. It measures the OUT frequency for RED, GREEN and BLUE by counting rising
   edges (Input Capture 1) over a hardware-timed gate (Timer1):
   `freq = edges / gate_seconds`.
3. It forms `q = fR / fG` and inverts the Wien two-colour model
   `q = K·exp(B/T)` → `T = B / ln(q/K)`.
4. `B` comes from the channel wavelengths; `K` comes from **calibration**
   (see below). It prints the channel frequencies, `q`, and the temperature.

## Hardware

- dsPIC33 (reference: **dsPIC33EP128GP502**, 28-pin, 3.3 V) + programmer
  (PICkit/ICD).
- TCS3200 module (power it at **3.3 V** to match the dsPIC logic).
- 3.3 V USB-serial adapter for the UART output.
- Optics: an aperture/short tube and ideally an IR-cut filter; heat shielding.

Full pin map and optical/safety notes: **[docs/wiring.md](docs/wiring.md)**.

## Build

Uses the Microchip **XC16** compiler.

```sh
make            # -> build/tcs3200_pyrometer.hex
make clean
```

Set `DEVICE` in the [`Makefile`](Makefile) to your part. You can also just add
the files under `src/` to an MPLAB X project and build there. Flash the `.hex`
with MPLAB X / IPE and a PICkit.

## Use

1. Wire it up per [docs/wiring.md](docs/wiring.md); keep the module's white
   **LEDs off**.
2. Open a serial terminal at **115200 8N1**. You'll see lines like:
   ```
   R=4123Hz G=7841Hz B=9012Hz  q=0.526  T=982.3C
   ```
3. **Calibrate** against a known temperature so the readings are real degrees —
   until then they're tagged `[UNCALIBRATED - relative only]`. See
   **[docs/calibration.md](docs/calibration.md)**.

## Project layout

| File | Purpose |
|------|---------|
| `src/main.c`       | Measurement loop, validity checks, UART output |
| `src/tcs3200.c/.h` | Sensor driver: S0–S3 control, edge-count frequency measure |
| `src/pyrometer.c/.h` | Colour-ratio → temperature model + calibration |
| `src/board.c/.h`   | Config bits, clock, PPS, UART, pin map |
| `docs/wiring.md`   | Pin map, levels, optics, safety |
| `docs/calibration.md` | One- and two-point calibration procedure |

## Limitations & honesty

- Only for **glowing** objects; nothing below visible glow.
- The TCS3200 filters are broad/overlapping, so this is an **approximate**
  pyrometer — accuracy depends on calibration and staying near the calibrated
  range.
- Near-IR sensitivity biases the ratio; an IR-cut filter helps.
- Pin numbers, PPS codes, config bits and the clock setup are for the reference
  dsPIC33EP part — **adjust them for your exact device**.
- Keep the sensor cool (≤ ~70 °C) and use eye protection around hot sources.
