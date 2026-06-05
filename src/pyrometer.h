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
