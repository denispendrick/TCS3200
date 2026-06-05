# Measuring temperature with a TCS3200 and a dsPIC33

## Principle

The TCS3200 isn't a thermometer. It's a colour-to-frequency chip — light hits its
photodiode array and it puts out a square wave whose frequency tracks how bright a
given colour is. Pick a filter (red, green, blue, or clear), get a frequency.
Nothing in there measures heat.

So how do you get temperature out of it? You don't, not directly. You measure the
*glow*. Anything hot enough radiates light, and the colour of that light depends on
temperature — dull red, then orange, then yellow, then white as it climbs. That's
blackbody radiation, and the spectrum slides toward blue the hotter things get.

The trick is to read two colours and compare them. Here it's red and green. As the
object heats up, green climbs faster than red, so the red/green ratio drops in a
predictable way. Working with the ratio instead of raw brightness is the whole
point: it cancels out how far away the object is and how shiny it is. That's why
real furnaces use two-colour (ratio) pyrometers.

One honest caveat — this only works once something actually glows, roughly 600 °C
and up. Below that there's almost no visible light to read.

## Configuration

The hardware is simple: a dsPIC33, a TCS3200 breakout, and a USB-serial cable to
watch the output.

Reference part is a **dsPIC33EP128GP502** (28-pin, 3.3 V, with PPS). Power the
sensor at **3.3 V** so its logic and the OUT swing match the dsPIC directly — no
level shifters needed. And keep the module's white LEDs **off**; you want the
object's own glow, not reflected light.

Two pairs of pins drive the sensor. **S0/S1** set the output scaling:

| S0 | S1 | Scaling     |
|----|----|-------------|
| 0  | 0  | Power down  |
| 0  | 1  | 2%          |
| 1  | 0  | 20%         |
| 1  | 1  | 100%        |

**S2/S3** pick the colour filter:

| S2 | S3 | Filter |
|----|----|--------|
| 0  | 0  | Red    |
| 0  | 1  | Blue   |
| 1  | 0  | Clear  |
| 1  | 1  | Green  |

Lower scaling keeps the output frequency — and the interrupt load — sane for bright
sources, so 2% is the default here.

On the dsPIC side, four peripherals do the work:

- **Input Capture 1** catches each rising edge of OUT.
- **Timer1** gives a fixed gate window, so we count edges over a known time.
- **UART1** streams the result at 115200 8N1.
- **FRC + PLL** for the clock (~60 MIPS).

Pin map (reference part; change it in `src/board.h` if yours differs):

| Signal     | dsPIC pin   | Note                 |
|------------|-------------|----------------------|
| S0..S3     | RB0..RB3    | scaling + filter     |
| /OE        | RB4         | or tie to GND        |
| LED        | RB5         | keep low (off)       |
| OUT        | RB7 / RP39  | → Input Capture 1    |
| UART TX    | RB6 / RP38  | → PC                 |
| UART RX    | RB8 / RP40  | optional             |

One safety note since this points at glowing-hot things: the TCS3200 is only rated
to about 70 °C ambient, so stand it back from the heat, use a small aperture or
lens, and wear eye protection around furnaces or molten metal.

## Circuit diagram

```
      3.3 V ──┬───────────────────────────┬─── VCC
              │                           │
        ┌─────┴───────────┐         ┌─────┴───────────┐
        │    dsPIC33      │         │     TCS3200     │
        │                 │         │                 │
        │  RB0 ───────────┼────────►│ S0  ┐ scaling   │
        │  RB1 ───────────┼────────►│ S1  ┘           │
        │  RB2 ───────────┼────────►│ S2  ┐ filter    │
        │  RB3 ───────────┼────────►│ S3  ┘           │
        │  RB4 ───────────┼────────►│ /OE  (or GND)   │
        │  RB5 ───────────┼────────►│ LED  (off)      │
        │  RB7/RP39  ◄────┼─────────┤ OUT  (freq)     │
        │                 │         └─────┬───────────┘
        │  RB6/RP38  TX ──┼──┐            │
        │  RB8/RP40  RX ◄─┼─┐│            │
        └────────┬────────┘ ││            │
                GND ────────┼┼────────────┴── GND (common)
                            ││
                      ┌─────┴┴─────┐
                      │ USB-serial │ ──► PC terminal, 115200 8N1
                      └────────────┘
```

## Algorithm

Nothing fancy, just a steady loop:

1. Bring up the clock, pins, Input Capture, the gate timer, and UART.
2. For each colour — red, green, blue:
   - set S2/S3,
   - wait a few ms to settle,
   - count OUT's rising edges for one gate window,
   - frequency = edges ÷ gate time.
3. Take the ratio `q = red ÷ green`.
4. Invert the Wien two-colour model to get temperature:
   `q = K · exp(B / T)`  →  `T = B / ln(q / K)`.
   `B` comes from the two channel wavelengths; `K` comes from calibration.
5. Sanity-check the result (too dim means no glow, too bright means saturation),
   smooth it a little, and print.

About `K`: it folds in how the two channels actually respond, and it's large, so an
uncalibrated reading is only relative. One known temperature pins it down
(one-point); two known points fit both `B` and `K` and do better. The steps are in
the code comments.

The frequency measurement is worth one line: rather than lean on the dsPIC33's
input-capture timing modes (which differ between sub-families), it just counts
rising edges over a hardware-timed Timer1 window. Count ÷ time — portable, and easy
to trust.

## Program

File layout:

- `src/main.c` — the loop above: measure, convert, print.
- `src/tcs3200.c/.h` — sensor driver: drive S0–S3, measure OUT by gated edge count.
- `src/pyrometer.c/.h` — ratio → temperature, with one/two-point calibration
  (this part is host-tested).
- `src/board.c/.h` — clock, PPS, UART, config bits, pin map.
- `Makefile` — XC16 build.

Full source follows.

### `src/board.h`

```c
/*
 * board.h - Board / peripheral configuration for the TCS3200 pyrometer.
 *
 * Target reference part: dsPIC33EP128GP502 (28-pin, PPS-capable, 3.3 V).
 * Adjust pin macros, RPn numbers and the clock setup for your exact device
 * and package - the RPn assignments below assume RB0..RB15 == RP32..RP47.
 *
 * NOTE: this firmware does NOT measure temperature directly. The TCS3200 is a
 * light-to-frequency converter; we read its colour channels and infer the
 * temperature of a *glowing* object by two-colour pyrometry (see pyrometer.c).
 */
#ifndef BOARD_H
#define BOARD_H

#include <xc.h>
#include <stdint.h>

/* Instruction clock after clock_init(). FRC(7.37 MHz)+PLL -> ~120 MHz FOSC.
 * Actual FCY is ~59.9 MHz (FRC tolerance); 60e6 is close enough for timing
 * and UART. For accurate UART at high baud, use an external crystal. */
#define FCY 60000000UL

/* ----------------------------------------------------------------------------
 * TCS3200 control pins (digital outputs)
 *   S0,S1 -> output-frequency scaling      S2,S3 -> colour-filter select
 * Change these to match your wiring.
 * ------------------------------------------------------------------------- */
#define S0_TRIS  TRISBbits.TRISB0
#define S0_LAT   LATBbits.LATB0
#define S1_TRIS  TRISBbits.TRISB1
#define S1_LAT   LATBbits.LATB1
#define S2_TRIS  TRISBbits.TRISB2
#define S2_LAT   LATBbits.LATB2
#define S3_TRIS  TRISBbits.TRISB3
#define S3_LAT   LATBbits.LATB3

/* /OE - active-low output enable. Drive low to enable (or tie OE to GND). */
#define OE_TRIS  TRISBbits.TRISB4
#define OE_LAT   LATBbits.LATB4

/* White-LED enable on common TCS3200 breakout modules (HIGH = LEDs on).
 * Keep the LEDs OFF for emission pyrometry - we measure the object's own glow,
 * not reflected light. */
#define LED_TRIS TRISBbits.TRISB5
#define LED_LAT  LATBbits.LATB5

/* TCS3200 OUT -> Input Capture 1 input. OUT wired to RB7 == RP39. */
#define OUT_TRIS TRISBbits.TRISB7
#define OUT_RPIN 39          /* RPn fed to _IC1R */

/* UART1 (results). U1TX on RB6 == RP38, U1RX on RB8 == RP40 (RX optional). */
#define UART_BAUD  115200UL
#define U1RX_RPIN  40

/* Measurement gate (Timer1, 1:256 prescale). Longer gate = finer resolution
 * but slower updates. Keep <= ~279 ms so PR1 stays within 16 bits at this
 * prescale and FCY. */
#define GATE_MS    100UL

/* Settling time after switching the colour filter, before counting. */
#define SETTLE_MS  5

void   clock_init(void);
void   board_gpio_init(void);
void   pps_init(void);

void   uart_init(void);
void   uart_putc(char c);
void   uart_puts(const char *s);
void   uart_put_u32(uint32_t v);
void   uart_put_double(double v, uint8_t decimals);

#endif /* BOARD_H */
```

### `src/board.c`

```c
/*
 * board.c - configuration bits, clock, PPS mapping and a tiny UART driver.
 * Reference device: dsPIC33EP128GP502. Verify every config key/pin for yours.
 */
#include "board.h"
#include <math.h>   /* isnan() */

/* --------------------------- Configuration bits --------------------------- */
/* These keys are for the dsPIC33EP family; confirm against your device's
 * "Configuration Bits" reference (MPLAB X: Window > Target Memory Views). */
#pragma config FNOSC   = FRC        /* start on FRC; we switch to FRCPLL in code */
#pragma config IESO    = OFF
#pragma config POSCMD  = NONE       /* no primary (crystal) oscillator          */
#pragma config OSCIOFNC = ON        /* OSC2 pin is general-purpose I/O           */
#pragma config IOL1WAY = OFF        /* allow PPS to be (re)configured            */
#pragma config FCKSM   = CSECMD     /* clock switching enabled, monitor off      */
#pragma config FWDTEN  = OFF        /* watchdog off                              */
#pragma config ICS     = PGD1       /* ICSP on PGEC1/PGED1                        */
#pragma config JTAGEN  = OFF
#pragma config GWRP    = OFF
#pragma config GSS     = OFF

/* ------------------------------- Clock ------------------------------------ */
void clock_init(void)
{
    /* FOSC = FIN * M / (N1 * N2)
     * FIN = 7.37 MHz (FRC), N1 = 2 (PLLPRE=0), M = 65 (PLLFBD=63), N2 = 2.
     * FVCO = 7.37/2 * 65 = 239.5 MHz (in 120-340 MHz range)
     * FOSC = 239.5 / 2 = ~119.8 MHz  ->  FCY ~= 59.9 MHz */
    PLLFBD = 63;                 /* M  = PLLFBD + 2 = 65 */
    CLKDIVbits.PLLPOST = 0;      /* N2 = 2               */
    CLKDIVbits.PLLPRE  = 0;      /* N1 = 2               */

    __builtin_write_OSCCONH(0x01);                       /* NOSC = FRCPLL */
    __builtin_write_OSCCONL((uint8_t)(OSCCON | 0x01));   /* OSWEN = 1     */
    while (OSCCONbits.OSWEN)   { }   /* wait for switch to complete */
    while (OSCCONbits.LOCK != 1) { } /* wait for PLL lock           */
}

void board_gpio_init(void)
{
    /* Make all of PORTA/PORTB digital (we use no analog inputs). */
    ANSELA = 0x0000;
    ANSELB = 0x0000;
}

/* ----------------------------- PPS mapping -------------------------------- */
void pps_init(void)
{
    __builtin_write_OSCCONL((uint8_t)(OSCCON & ~(1 << 6)));  /* IOLOCK = 0 */

    _IC1R   = OUT_RPIN;     /* IC1 input  <- TCS3200 OUT (RP39) */
    _U1RXR  = U1RX_RPIN;    /* U1RX input <- RP40 (optional)    */
    _RP38R  = 0b000001;     /* RP38 output -> U1TX (func 1)     */

    __builtin_write_OSCCONL((uint8_t)(OSCCON | (1 << 6)));   /* IOLOCK = 1 */
}

/* ------------------------------- UART1 ------------------------------------ */
void uart_init(void)
{
    TRISBbits.TRISB6 = 0;   /* U1TX output */
    TRISBbits.TRISB8 = 1;   /* U1RX input  */

    U1MODE = 0;
    U1MODEbits.BRGH = 1;                                  /* 4x mode */
    U1BRG  = (uint16_t)((FCY / (4UL * UART_BAUD)) - 1UL);
    U1STA  = 0;
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN   = 1;
}

void uart_putc(char c)
{
    while (U1STAbits.UTXBF) { }   /* wait while TX buffer full */
    U1TXREG = c;
}

void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

void uart_put_u32(uint32_t v)
{
    char buf[11];
    int  i = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i--) uart_putc(buf[i]);
}

/* Print a double with a fixed number of decimals (0..4). Handles NaN/sign. */
void uart_put_double(double v, uint8_t decimals)
{
    if (isnan(v)) { uart_puts("nan"); return; }
    if (v < 0)    { uart_putc('-'); v = -v; }

    double scale = 1.0;
    for (uint8_t i = 0; i < decimals; i++) scale *= 10.0;

    uint32_t ip   = (uint32_t)v;
    uint32_t fp   = (uint32_t)((v - (double)ip) * scale + 0.5);
    if (fp >= (uint32_t)scale) { ip++; fp -= (uint32_t)scale; }  /* carry */

    uart_put_u32(ip);
    if (decimals) {
        uart_putc('.');
        uint32_t div = (uint32_t)scale / 10;
        while (div > 0) { uart_putc((char)('0' + (fp / div) % 10)); div /= 10; }
    }
}
```

### `src/tcs3200.h`

```c
/*
 * tcs3200.h - TCS3200 colour-to-frequency sensor driver (dsPIC33, XC16).
 *
 * Drives S0/S1 (frequency scaling) and S2/S3 (colour filter) over GPIO and
 * measures the OUT square-wave frequency by counting rising edges (Input
 * Capture 1) over a hardware-timed gate (Timer1).
 */
#ifndef TCS3200_H
#define TCS3200_H

#include <stdint.h>

/* Colour filter (S2,S3) - per TCS3200 datasheet truth table:
 *   RED   : S2=0 S3=0      BLUE  : S2=0 S3=1
 *   CLEAR : S2=1 S3=0      GREEN : S2=1 S3=1                                  */
typedef enum {
    TCS_RED = 0,
    TCS_GREEN,
    TCS_BLUE,
    TCS_CLEAR
} tcs_color_t;

/* Output-frequency scaling (S0,S1):
 *   PWRDOWN: 0,0   2%: 0,1   20%: 1,0   100%: 1,1
 * Lower scaling keeps OUT frequency (and the per-edge interrupt rate) modest
 * for bright, glowing sources - 2% is a good default for pyrometry.          */
typedef enum {
    TCS_SCALE_PWRDOWN = 0,
    TCS_SCALE_2,
    TCS_SCALE_20,
    TCS_SCALE_100
} tcs_scale_t;

void   tcs3200_init(void);                 /* pins, PPS-fed IC1, gate timer    */
void   tcs3200_set_scale(tcs_scale_t s);
void   tcs3200_set_color(tcs_color_t c);

/* Select colour, settle, then return OUT frequency in Hz (0 if no edges). */
double tcs3200_measure_hz(tcs_color_t c);

#endif /* TCS3200_H */
```

### `src/tcs3200.c`

```c
/*
 * tcs3200.c - TCS3200 driver implementation.
 *
 * Frequency measurement strategy (portable across dsPIC33 families):
 *   - Input Capture 1 is set to capture every rising edge of OUT and to
 *     interrupt on each capture. The ISR just counts edges (we ignore the
 *     captured timer value), so no family-specific IC time-base wiring is
 *     needed - only PPS + ICM are used.
 *   - Timer1 (1:256) defines a fixed, hardware-accurate gate. Counting edges
 *     over a known time window gives frequency = edges / gate_seconds. Because
 *     the gate is timed in hardware, CPU time stolen by the ISR does not bias
 *     the result.
 *
 * Keep OUT frequency below a few tens of kHz (use 2%/20% scaling) so the
 * per-edge ISR can keep up; otherwise the IC FIFO overruns and counts low.
 */
#include "board.h"
#include "tcs3200.h"
#include <libpic30.h>   /* __delay_ms - requires FCY (from board.h) */

/* Timer1 gate reload: ticks = (FCY / prescale) * GATE_MS / 1000. */
#define T1_PRESCALE 256UL
#define GATE_TICKS  ((uint16_t)(((FCY / T1_PRESCALE) * GATE_MS) / 1000UL))

static volatile uint32_t g_edges;   /* rising edges counted during the gate */

void __attribute__((interrupt, no_auto_psv)) _IC1Interrupt(void)
{
    /* Drain the capture FIFO, counting every queued rising edge. Reading
     * IC1BUF clears the buffer-not-empty / overflow condition. */
    while (IC1CON1bits.ICBNE) { (void)IC1BUF; g_edges++; }
    _IC1IF = 0;
}

void tcs3200_set_scale(tcs_scale_t s)
{
    switch (s) {
        case TCS_SCALE_PWRDOWN: S0_LAT = 0; S1_LAT = 0; break;
        case TCS_SCALE_2:       S0_LAT = 0; S1_LAT = 1; break;
        case TCS_SCALE_20:      S0_LAT = 1; S1_LAT = 0; break;
        case TCS_SCALE_100:     S0_LAT = 1; S1_LAT = 1; break;
    }
}

void tcs3200_set_color(tcs_color_t c)
{
    switch (c) {
        case TCS_RED:   S2_LAT = 0; S3_LAT = 0; break;
        case TCS_BLUE:  S2_LAT = 0; S3_LAT = 1; break;
        case TCS_CLEAR: S2_LAT = 1; S3_LAT = 0; break;
        case TCS_GREEN: S2_LAT = 1; S3_LAT = 1; break;
    }
}

void tcs3200_init(void)
{
    /* Control pins as outputs; OUT as input. */
    S0_TRIS = 0; S1_TRIS = 0; S2_TRIS = 0; S3_TRIS = 0;
    OE_TRIS = 0; LED_TRIS = 0;
    OUT_TRIS = 1;

    OE_LAT  = 0;   /* enable sensor output (/OE active low) */
    LED_LAT = 0;   /* white LEDs OFF: we measure the object's own glow */

    tcs3200_set_scale(TCS_SCALE_2);   /* sensible default for bright sources */
    tcs3200_set_color(TCS_CLEAR);

    /* Input Capture 1: capture every rising edge, interrupt every capture. */
    IC1CON1 = 0;
    IC1CON2 = 0;                 /* no sync/trigger source (free running)    */
    IC1CON1bits.ICI = 0b00;      /* interrupt on every capture               */
    IC1CON1bits.ICM = 0b011;     /* capture mode: every rising edge          */
    _IC1IP = 4;
    _IC1IF = 0;
    _IC1IE = 0;                  /* enabled only during a measurement gate   */

    /* Timer1 as the gate time-base (internal clock, 1:256). Polled, no ISR. */
    T1CON = 0;
    T1CONbits.TCKPS = 0b11;      /* 1:256 prescale */
    PR1 = GATE_TICKS;
    _T1IF = 0;
}

double tcs3200_measure_hz(tcs_color_t c)
{
    tcs3200_set_color(c);
    __delay_ms(SETTLE_MS);

    /* Flush any stale captures from the previous colour, then arm. */
    _IC1IE = 0;
    while (IC1CON1bits.ICBNE) { (void)IC1BUF; }
    g_edges = 0;
    _IC1IF  = 0;

    TMR1   = 0;
    _T1IF  = 0;
    T1CONbits.TON = 1;           /* start the gate */
    _IC1IE = 1;                  /* start counting edges */

    while (!_T1IF) { }           /* busy-wait the hardware-timed gate */

    _IC1IE = 0;
    T1CONbits.TON = 0;

    uint32_t edges = g_edges;
    return (double)edges * (1000.0 / (double)GATE_MS);
}
```

### `src/pyrometer.h`

```c
/*
 * pyrometer.h - two-colour (ratio) pyrometry: colour -> temperature.
 *
 * A glowing object emits a blackbody spectrum that shifts toward blue as it
 * gets hotter. The ratio of two colour channels (here RED/GREEN from the
 * TCS3200) is a monotonic function of temperature and is largely independent
 * of distance and emissivity - the basis of two-colour pyrometry.
 *
 * Model (Wien approximation):
 *     q = fR/fG = K * exp(B / T)         with  B = c2 * (1/lambdaG - 1/lambdaR)
 *  => T = B / ln(q / K)
 *
 * B comes from the channel wavelengths (physics). K lumps the relative channel
 * responsivities and MUST be found by calibration against a known temperature -
 * an uncalibrated reading is only a relative indication, not real degrees.
 */
#ifndef PYROMETER_H
#define PYROMETER_H

/* Approximate effective wavelengths of the TCS3200 RED/GREEN channels (nm).
 * The real filters are broad and overlapping - tune these (and prefer a
 * two-point calibration) for best accuracy. */
#define PYRO_LAMBDA_R_NM 615.0
#define PYRO_LAMBDA_G_NM 525.0

typedef struct {
    double K;   /* responsivity/geometry factor (from calibration) */
    double B;   /* slope term, kelvin (from wavelengths or 2-pt fit) */
} pyro_cal_t;

/* Physics-only defaults: B from wavelengths, K = 1 (UNCALIBRATED). */
void   pyro_cal_default(pyro_cal_t *c);

/* Keep physics B, solve K so the curve passes through one known point. */
void   pyro_cal_one_point(pyro_cal_t *c, double Tref_K, double q_ref);

/* Solve both B and K from two known (temperature, ratio) points. */
void   pyro_cal_two_point(pyro_cal_t *c,
                          double T1_K, double q1,
                          double T2_K, double q2);

double pyro_ratio(double fR, double fG);              /* fR/fG (0 if fG<=0) */
double pyro_temp_K(const pyro_cal_t *c, double q);    /* NaN if out of range */

static inline double pyro_K2C(double k) { return k - 273.15; }
static inline double pyro_C2K(double c) { return c + 273.15; }

#endif /* PYROMETER_H */
```

### `src/pyrometer.c`

```c
/*
 * pyrometer.c - two-colour pyrometry math (see pyrometer.h for the model).
 */
#include "pyrometer.h"
#include <math.h>

/* Second radiation constant c2 = 1.438777e-2 m*K, expressed in nm*K. */
#define C2_NM_K 1.438777e7

static double slope_from_wavelengths(void)
{
    /* B = c2 * (1/lambdaG - 1/lambdaR); > 0 since lambdaG < lambdaR. */
    return C2_NM_K * (1.0 / PYRO_LAMBDA_G_NM - 1.0 / PYRO_LAMBDA_R_NM);
}

void pyro_cal_default(pyro_cal_t *c)
{
    c->B = slope_from_wavelengths();
    c->K = 1.0;                 /* placeholder - calibrate before trusting */
}

void pyro_cal_one_point(pyro_cal_t *c, double Tref_K, double q_ref)
{
    c->B = slope_from_wavelengths();
    c->K = q_ref / exp(c->B / Tref_K);
}

void pyro_cal_two_point(pyro_cal_t *c,
                        double T1_K, double q1,
                        double T2_K, double q2)
{
    /* ln(q1/q2) = B (1/T1 - 1/T2)  ->  B,  then K from point 1. */
    c->B = log(q1 / q2) / (1.0 / T1_K - 1.0 / T2_K);
    c->K = q1 / exp(c->B / T1_K);
}

double pyro_ratio(double fR, double fG)
{
    return (fG > 0.0) ? (fR / fG) : 0.0;
}

double pyro_temp_K(const pyro_cal_t *c, double q)
{
    if (q <= 0.0 || c->K <= 0.0) return NAN;
    double r = q / c->K;
    if (r <= 1.0) return NAN;   /* q<=K: object hotter than the model allows,
                                   or miscalibration / saturation */
    return c->B / log(r);
}
```

### `src/main.c`

```c
/*
 * main.c - TCS3200 two-colour pyrometer on dsPIC33.
 *
 * Reads RED/GREEN/BLUE frequencies from the TCS3200, forms the RED/GREEN
 * ratio, and converts it to a temperature by two-colour pyrometry. Results
 * are streamed over UART1 (115200 8N1).
 *
 * Reminder: this only works for objects hot enough to glow visibly
 * (~600 C and up). Keep the module's white LEDs OFF.
 */
#include "board.h"
#include "tcs3200.h"
#include "pyrometer.h"
#include <libpic30.h>
#include <stdbool.h>
#include <math.h>

/* ===== CALIBRATION ==========================================================
 * Uncomment and fill these in from docs/calibration.md (one-point is the
 * minimum for real degrees; pyro_cal_two_point() in main() is even better).
 *   PYRO_ONE_POINT_TC : reference temperature in degrees C
 *   PYRO_ONE_POINT_Q  : the fR/fG ratio measured at that temperature
 * ========================================================================== */
/* #define PYRO_ONE_POINT_TC  1000.0 */
/* #define PYRO_ONE_POINT_Q   0.620  */

/* Validity window for the colour-channel frequencies (Hz). */
#define MIN_HZ  30.0        /* below this: too cold/dim or not glowing */
#define SAT_HZ  90000.0     /* above this: saturated/too close/too bright */

static void print_reading(double fR, double fG, double fB,
                          double q, double tempC, bool calibrated)
{
    uart_puts("R=");  uart_put_double(fR, 0); uart_puts("Hz ");
    uart_puts("G=");  uart_put_double(fG, 0); uart_puts("Hz ");
    uart_puts("B=");  uart_put_double(fB, 0); uart_puts("Hz  ");
    uart_puts("q=");  uart_put_double(q, 3);  uart_puts("  ");

    if (isnan(tempC)) {
        uart_puts("T=-- (out of range)");
    } else {
        uart_puts("T=");
        uart_put_double(tempC, 1);
        uart_puts("C");
    }
    if (!calibrated) uart_puts("  [UNCALIBRATED - relative only]");
    uart_puts("\r\n");
}

int main(void)
{
    clock_init();
    board_gpio_init();
    pps_init();
    uart_init();
    tcs3200_init();

    pyro_cal_t cal;
#if defined(PYRO_ONE_POINT_TC) && defined(PYRO_ONE_POINT_Q)
    pyro_cal_one_point(&cal, pyro_C2K(PYRO_ONE_POINT_TC), PYRO_ONE_POINT_Q);
    bool calibrated = true;
#else
    pyro_cal_default(&cal);
    bool calibrated = false;
#endif

    uart_puts("\r\nTCS3200 two-colour pyrometer\r\n");
    uart_puts("Measures glowing objects (~600C+). Keep module LEDs OFF.\r\n\r\n");

    double tFiltC = NAN;        /* exponential smoothing of the temperature */

    for (;;) {
        double fR = tcs3200_measure_hz(TCS_RED);
        double fG = tcs3200_measure_hz(TCS_GREEN);
        double fB = tcs3200_measure_hz(TCS_BLUE);

        bool   valid = (fR >= MIN_HZ && fG >= MIN_HZ) &&
                       (fR <= SAT_HZ && fG <= SAT_HZ && fB <= SAT_HZ);

        double q     = pyro_ratio(fR, fG);
        double tempC = NAN;

        if (valid) {
            double tK = pyro_temp_K(&cal, q);
            if (!isnan(tK)) {
                double c = pyro_K2C(tK);
                tFiltC = isnan(tFiltC) ? c : (0.5 * c + 0.5 * tFiltC);
                tempC  = tFiltC;
            }
        } else {
            tFiltC = NAN;       /* reset the filter when we lose the target */
        }

        print_reading(fR, fG, fB, q, tempC, calibrated);
        __delay_ms(400);
    }
    return 0;
}
```

### `Makefile`

```makefile
# Build the TCS3200 pyrometer firmware with the Microchip XC16 compiler.
#
#   make            # build build/tcs3200_pyrometer.hex
#   make clean
#
# Requires xc16-gcc / xc16-bin2hex on PATH (MPLAB XC16 install).
# Change DEVICE to match your dsPIC33.

DEVICE   = 33EP128GP502
MCU      = -mcpu=$(DEVICE)

CC       = xc16-gcc
BIN2HEX  = xc16-bin2hex

BUILD    = build
TARGET   = $(BUILD)/tcs3200_pyrometer

SRC      = $(wildcard src/*.c)
OBJ      = $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))

CFLAGS   = $(MCU) -O1 -Wall -Wextra -Isrc

all: $(TARGET).hex

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(MCU) -o $@ $(OBJ) -Wl,--report-mem

$(TARGET).hex: $(TARGET).elf
	$(BIN2HEX) $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
```
