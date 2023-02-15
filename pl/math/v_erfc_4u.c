/*
 * Double-precision vector erfc(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "horner.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

/* Accurate exponential (vector variant of exp_dd).  */
float64x2_t __v_exp_tail (float64x2_t, float64x2_t);

#define One v_f64 (1.0)
#define AbsMask v_u64 (0x7fffffffffffffff)
#define Scale v_f64 (0x1.0000002p27)

/* Coeffs for polynomial approximation on [0x1.0p-28., 31.].  */
#define PX __v_erfc_data.poly
#define xint __v_erfc_data.interval_bounds

/* Special cases (fall back to scalar calls).  */
VPCS_ATTR
NOINLINE static float64x2_t
specialcase (float64x2_t x, float64x2_t y, uint64x2_t cmp)
{
  return v_call_f64 (erfc, x, y, cmp);
}

/* A structure to perform look-up in coeffs and other parameter
   tables.  */
struct entry
{
  float64x2_t P[ERFC_POLY_ORDER + 1];
  float64x2_t xi;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  for (int j = 0; j <= ERFC_POLY_ORDER; ++j)
    {
      e.P[j][0] = PX[i[0]][j];
      e.P[j][1] = PX[i[1]][j];
    }
  e.xi[0] = xint[i[0]];
  e.xi[1] = xint[i[1]];
  return e;
}

/* Accurate evaluation of exp(x^2) using compensated product
   (x^2 ~ x*x + e2) and custom exp(y+d) routine for small
   corrections d<<y.  */
static inline float64x2_t
v_eval_gauss (float64x2_t a)
{
  float64x2_t e2;
  float64x2_t a2 = a * a;

  /* TwoProduct (Dekker) applied to a * a.  */
  float64x2_t a_hi = -vfmaq_f64 (-a, Scale, a);
  a_hi = vfmaq_f64 (a_hi, Scale, a);
  float64x2_t a_lo = a - a_hi;

  /* Now assemble error term.  */
  e2 = vfmaq_f64 (a2, -a_hi, a_hi);
  e2 = vfmaq_f64 (e2, -a_hi, a_lo);
  e2 = vfmaq_f64 (e2, -a_lo, a_hi);
  e2 = vfmaq_f64 (e2, -a_lo, a_lo);

  /* Fast and accurate evaluation of exp(-a2 + e2) where e2 << a2.  */
  return __v_exp_tail (-a2, e2);
}

/* Optimized double precision vector complementary error function erfc.
   Maximum measured error is 3.64 ULP:
   __v_erfc(0x1.4792573ee6cc7p+2) got 0x1.ff3f4c8e200d5p-42
				 want 0x1.ff3f4c8e200d9p-42.  */
VPCS_ATTR
float64x2_t V_NAME_D1 (erfc) (float64x2_t x)
{
  float64x2_t z, p, y;
  uint64x2_t ix, atop, sign, i, cmp;

  ix = vreinterpretq_u64_f64 (x);
  /* Compute fac as early as possible in order to get best performance.  */
  float64x2_t fac = vreinterpretq_f64_u64 ((ix >> 63) << 62);
  /* Use 12-bit for small, nan and inf case detection.  */
  atop = (ix >> 52) & 0x7ff;
  cmp = atop - v_u64 (0x3cd) >= v_u64 (0x7ff - 0x3cd);

  struct entry dat;

  /* All entries of the vector are out of bounds, take a short path.
     Use smallest possible number above 28 representable in 12 bits.  */
  uint64x2_t out_of_bounds = atop >= v_u64 (0x404);

  /* Use sign to produce either 0 if x > 0, 2 otherwise.  */
  if (v_all_u64 (out_of_bounds) && likely (v_any_u64 (~cmp)))
    return fac;

  /* erfc(|x|) = P(|x|-x_i)*exp(-x^2).  */

  float64x2_t a = vabsq_f64 (x);

  /* Interval bounds are a logarithmic scale, i.e. interval n has
     lower bound 2^(n/4) - 1. Use the exponent of (|x|+1)^4 to obtain
     the interval index.  */
  float64x2_t xp1 = a + v_f64 (1.0);
  xp1 = xp1 * xp1;
  xp1 = xp1 * xp1;
  uint64x2_t ixp1 = vreinterpretq_u64_f64 (xp1);
  i = (ixp1 >> 52) - v_u64 (1023);

  /* Index cannot exceed number of polynomials.  */
  i = (uint64x2_t){i[0] <= ERFC_NUM_INTERVALS ? i[0] : ERFC_NUM_INTERVALS,
		   i[1] <= ERFC_NUM_INTERVALS ? i[1] : ERFC_NUM_INTERVALS};

  /* Get coeffs of i-th polynomial.  */
  dat = lookup (i);

  /* Evaluate Polynomial: P(|x|-x_i).  */
  z = a - dat.xi;
#define C(i) dat.P[i]
  p = HORNER_12 (z, C);

  /* Evaluate Gaussian: exp(-x^2).  */
  float64x2_t e = v_eval_gauss (a);

  /* Copy sign.  */
  sign = vreinterpretq_u64_f64 (x) & ~AbsMask;
  p = vreinterpretq_f64_u64 (vreinterpretq_u64_f64 (p) ^ sign);

  /* Assemble result as 2.0 - p * e if x < 0, p * e otherwise.  */
  y = vfmaq_f64 (fac, p, e);

  /* No need to fix value of y if x is out of bound, as
     P[ERFC_NUM_INTERVALS]=0.  */
  if (unlikely (v_any_u64 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}

PL_SIG (V, D, 1, erfc, -6.0, 28.0)
PL_TEST_ULP (V_NAME_D1 (erfc), 3.15)
PL_TEST_INTERVAL (V_NAME_D1 (erfc), 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (V_NAME_D1 (erfc), 0x1p-1022, 0x1p-26, 40000)
PL_TEST_INTERVAL (V_NAME_D1 (erfc), -0x1p-1022, -0x1p-26, 40000)
PL_TEST_INTERVAL (V_NAME_D1 (erfc), 0x1p-26, 0x1p5, 40000)
PL_TEST_INTERVAL (V_NAME_D1 (erfc), -0x1p-26, -0x1p3, 40000)
PL_TEST_INTERVAL (V_NAME_D1 (erfc), 0, inf, 40000)
