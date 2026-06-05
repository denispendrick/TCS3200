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
