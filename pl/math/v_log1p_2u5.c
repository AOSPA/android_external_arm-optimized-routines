/*
 * Double-precision vector log(1+x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrin.h"
#include "pl_sig.h"
#include "pl_test.h"

#define Ln2Hi v_f64 (0x1.62e42fefa3800p-1)
#define Ln2Lo v_f64 (0x1.ef35793c76730p-45)
#define HfRt2Top 0x3fe6a09e00000000 /* top32(asuint64(sqrt(2)/2)) << 32.  */
#define OneMHfRt2Top                                                           \
  0x00095f6200000000 /* (top32(asuint64(1)) - top32(asuint64(sqrt(2)/2)))      \
			<< 32.  */
#define OneTop12 0x3ff
#define BottomMask 0xffffffff
#define AbsMask 0x7fffffffffffffff
#define C(i) v_f64 (__log1p_data.coeffs[i])

static inline float64x2_t
eval_poly (float64x2_t f)
{
  float64x2_t f2 = f * f;
  float64x2_t f4 = f2 * f2;
  float64x2_t f8 = f4 * f4;
  return ESTRIN_18 (f, f2, f4, f8, f8 * f8, C);
}

VPCS_ATTR
NOINLINE static float64x2_t
specialcase (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (log1p, x, y, special);
}

/* Vector log1p approximation using polynomial on reduced interval. Routine is a
   modification of the algorithm used in scalar log1p, with no shortcut for k=0
   and no narrowing for f and k. Maximum observed error is 2.46 ULP:
    __v_log1p(0x1.654a1307242a4p+11) got 0x1.fd5565fb590f4p+2
				    want 0x1.fd5565fb590f6p+2 .  */
VPCS_ATTR float64x2_t V_NAME_D1 (log1p) (float64x2_t x)
{
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t ia = ix & AbsMask;
  uint64x2_t special = (ia >= v_u64 (0x7ff0000000000000))
		       | (ix >= 0xbff0000000000000)
		       | (ix == 0x8000000000000000);

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u64 (special)))
    x = vbslq_f64 (special, v_f64 (0), x);
#endif

  /* With x + 1 = t * 2^k (where t = f + 1 and k is chosen such that f
			   is in [sqrt(2)/2, sqrt(2)]):
     log1p(x) = k*log(2) + log1p(f).

     f may not be representable exactly, so we need a correction term:
     let m = round(1 + x), c = (1 + x) - m.
     c << m: at very small x, log1p(x) ~ x, hence:
     log(1+x) - log(m) ~ c/m.

     We therefore calculate log1p(x) by k*log2 + log1p(f) + c/m.  */

  /* Obtain correctly scaled k by manipulation in the exponent.
     The scalar algorithm casts down to 32-bit at this point to calculate k and
     u_red. We stay in double-width to obtain f and k, using the same constants
     as the scalar algorithm but shifted left by 32.  */
  float64x2_t m = x + 1;
  uint64x2_t mi = vreinterpretq_u64_f64 (m);
  uint64x2_t u = mi + OneMHfRt2Top;

  int64x2_t ki = vreinterpretq_s64_u64 (u >> 52) - OneTop12;
  float64x2_t k = vcvtq_f64_s64 (ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  uint64x2_t utop = (u & 0x000fffff00000000) + HfRt2Top;
  uint64x2_t u_red = utop | (mi & BottomMask);
  float64x2_t f = vreinterpretq_f64_u64 (u_red) - 1;

  /* Correction term c/m.  */
  float64x2_t cm = (x - (m - 1)) / m;

  /* Approximate log1p(x) on the reduced input using a polynomial. Because
   log1p(0)=0 we choose an approximation of the form:
      x + C0*x^2 + C1*x^3 + C2x^4 + ...
   Hence approximation has the form f + f^2 * P(f)
      where P(x) = C0 + C1*x + C2x^2 + ...
   Assembling this all correctly is dealt with at the final step.  */
  float64x2_t p = eval_poly (f);

  float64x2_t ylo = vfmaq_f64 (cm, k, Ln2Lo);
  float64x2_t yhi = vfmaq_f64 (f, k, Ln2Hi);
  float64x2_t y = vfmaq_f64 (ylo + yhi, f * f, p);

  if (unlikely (v_any_u64 (special)))
    return specialcase (vreinterpretq_f64_u64 (ix), y, special);

  return y;
}

PL_SIG (V, D, 1, log1p, -0.9, 10.0)
PL_TEST_ULP (V_NAME_D1 (log1p), 1.97)
PL_TEST_EXPECT_FENV (V_NAME_D1 (log1p), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), -10.0, 10.0, 10000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), 0.0, 0x1p-23, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), 0x1p-23, 0.001, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), 0.001, 1.0, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), 0.0, -0x1p-23, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), -0x1p-23, -0.001, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), -0.001, -1.0, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), -1.0, inf, 5000)
