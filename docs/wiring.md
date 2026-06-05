# Wiring

Reference MCU: **dsPIC33EP128GP502** (28-pin). Pin macros live in
[`src/board.h`](../src/board.h) — change them there if you wire it differently
or use another part/package.

## Pin map

| TCS3200 pin | Function                | dsPIC33 pin | RPn  | Notes                                   |
|-------------|-------------------------|-------------|------|-----------------------------------------|
| VCC         | Power                   | 3V3         | —    | Power the sensor at **3.3 V** (see below)|
| GND         | Ground                  | VSS         | —    | Common ground                           |
| S0          | Freq scaling bit 0      | RB0         | —    | GPIO out                                |
| S1          | Freq scaling bit 1      | RB1         | —    | GPIO out                                |
| S2          | Colour-filter bit 0     | RB2         | —    | GPIO out                                |
| S3          | Colour-filter bit 1     | RB3         | —    | GPIO out                                |
| OE (/OE)    | Output enable (active L)| RB4         | —    | Drive low, **or tie OE to GND**         |
| LED         | White-LED enable        | RB5         | —    | Keep **LOW/off** for pyrometry          |
| OUT         | Frequency output        | RB7         | RP39 | → Input Capture 1 (`_IC1R = 39`)        |
| —           | UART1 TX (to PC)        | RB6         | RP38 | `_RP38R = 1`                            |
| —           | UART1 RX (optional)     | RB8         | RP40 | `_U1RXR = 40`                           |

Connect UART TX/RX (and GND) to a 3.3 V USB-serial adapter; open a terminal at
**115200 8N1**.

## Power / logic levels

The TCS3200 runs from 2.7–5.5 V. **Power it at 3.3 V** so its logic and the OUT
swing match the dsPIC33's 3.3 V I/O directly — no level shifters needed. If you
must run the sensor at 5 V, level-shift OUT down to 3.3 V before the dsPIC pin,
and remember the dsPIC's 3.3 V control outputs may sit near the sensor's input
threshold.

## Optics & protection (pyrometry)

- **LEDs OFF.** For emission pyrometry you measure the object's own glow, not
  reflected light. Disable the module's white LEDs (LED pin low / jumper off).
- **Keep the sensor cool.** The TCS3200 is rated to ~70 °C ambient. Stand it
  well back from the hot source, use a small aperture/lens, and shield it from
  radiant heat. Never let the package get hot.
- **Field of view.** Aim so the glowing target fills the sensor's view; a dark
  background and a simple tube/aperture cut stray light and reflections.
- **Near-IR.** Silicon photodiodes respond well into the near-IR, which biases
  the colour ratio. An IR-cut filter over the sensor improves consistency, and
  calibration absorbs the rest.

## Safety

Molten metal, kilns and furnaces emit intense heat and light. Use proper eye
protection, keep a safe distance, and protect the sensor and wiring from heat.
