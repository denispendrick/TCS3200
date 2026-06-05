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
