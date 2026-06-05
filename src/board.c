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
