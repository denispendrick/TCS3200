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
