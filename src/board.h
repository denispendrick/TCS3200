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
