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
