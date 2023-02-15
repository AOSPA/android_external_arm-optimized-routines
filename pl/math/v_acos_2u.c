/*
 * Double-precision vector acos(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrin.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AllMask v_u64 (0xffffffffffffffff)
#define AbsMask (0x7fffffffffffffff)
#define Half v_f64 (0.5)
#define PiOver2 v_f64 (0x1.921fb54442d18p+0)
#define Pi v_f64 (0x1.921fb54442d18p+1)
#define Two v_f64 (2.0)
#define One v_f64 (1.0)
#define MOne v_f64 (-1.0)
#define Halfu (0x3fe0000000000000)
#define Oneu (0x3ff0000000000000)
#define Small (0x3e50000000000000) /* 2^-53.  */

#define P(i) v_f64 (__asin_poly[i])

#if WANT_SIMD_EXCEPT
static NOINLINE float64x2_t
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (acos, x, y, special);
}
#endif

/* Double-precision implementation of vector acos(x).

   For |x| < Small, approximate acos(x) by pi/2 - x. Small = 2^-53 for correct
   rounding.
   If WANT_SIMD_EXCEPT = 0, Small = 0 and we proceed with the following
   approximation.

   For |x| in [Small, 0.5], use an order 11 polynomial P such that the final
   approximation of asin is an odd polynomial:

     acos(x) ~ pi/2 - (x + x^3 P(x^2)).

   The largest observed error in this region is 1.18 ulps,
   __v_acos(0x1.fbab0a7c460f6p-2) got 0x1.0d54d1985c068p+0
				 want 0x1.0d54d1985c069p+0.

   For |x| in [0.5, 1.0], use same approximation with a change of variable

     acos(x) = y + y * z * P(z), with  z = (1-x)/2 and y = sqrt(z).

   This approach is described in more details in scalar asin.

   The largest observed error in this region is 1.52 ulps,
   __v_acos(0x1.23d362722f591p-1) got 0x1.edbbedf8a7d6ep-1
				 want 0x1.edbbedf8a7d6cp-1.  */
VPCS_ATTR float64x2_t V_NAME_D1 (acos) (float64x2_t x)
{
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t ia = ix & AbsMask;

#if WANT_SIMD_EXCEPT
  /* A single comparison for One, Small and QNaN.  */
  uint64x2_t special = ia - Small > Oneu - Small;
  if (unlikely (v_any_u64 (special)))
    return special_case (x, x, AllMask);
#else
  /* Fixing sign of NaN when x < -1.0.  */
  ix = vbslq_u64 (x < MOne, v_u64 (0), ix);
#endif

  float64x2_t ax = vreinterpretq_f64_u64 (ia);
  uint64x2_t a_le_half = ia <= Halfu;

  /* Evaluate polynomial Q(x) = z + z * z2 * P(z2) with
     z2 = x ^ 2         and z = |x|     , if |x| < 0.5
     z2 = (1 - |x|) / 2 and z = sqrt(z2), if |x| >= 0.5.  */
  float64x2_t z2 = vbslq_f64 (a_le_half, x * x, vfmaq_f64 (Half, -Half, ax));
  float64x2_t z = vbslq_f64 (a_le_half, ax, vsqrtq_f64 (z2));

  /* Use a single polynomial approximation P for both intervals.  */
  float64x2_t z4 = z2 * z2;
  float64x2_t z8 = z4 * z4;
  float64x2_t z16 = z8 * z8;
  float64x2_t p = ESTRIN_11 (z2, z4, z8, z16, P);

  /* Finalize polynomial: z + z * z2 * P(z2).  */
  p = vfmaq_f64 (z, z * z2, p);

  /* acos(|x|) = pi/2 - sign(x) * Q(|x|), for  |x| < 0.5
	       = 2 Q(|x|)               , for  0.5 < x < 1.0
	       = pi - 2 Q(|x|)          , for -1.0 < x < -0.5.  */
  float64x2_t y = vreinterpretq_f64_u64 (
    vbslq_u64 (v_u64 (AbsMask), vreinterpretq_u64_f64 (p), ix));

  uint64x2_t sign = x < 0;
  float64x2_t off = vbslq_f64 (sign, Pi, v_f64 (0.0));
  float64x2_t mul = vbslq_f64 (a_le_half, -One, Two);
  float64x2_t add = vbslq_f64 (a_le_half, PiOver2, off);

  return vfmaq_f64 (add, mul, y);
}

PL_SIG (V, D, 1, acos, -1.0, 1.0)
PL_TEST_ULP (V_NAME_D1 (acos), 1.02)
PL_TEST_EXPECT_FENV (V_NAME_D1 (acos), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME_D1 (acos), 0, Small, 5000)
PL_TEST_INTERVAL (V_NAME_D1 (acos), Small, 0.5, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (acos), 0.5, 1.0, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (acos), 1.0, 0x1p11, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (acos), 0x1p11, inf, 20000)
PL_TEST_INTERVAL (V_NAME_D1 (acos), -0, -inf, 20000)
