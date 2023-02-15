/*
 * Single-precision vector exp(x) - 1 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#define Shift v_f32 (0x1.8p23f)
#define InvLn2 v_f32 (0x1.715476p+0f)
#define MLn2hi v_f32 (-0x1.62e4p-1f)
#define MLn2lo v_f32 (-0x1.7f7d1cp-20f)
#define AbsMask (0x7fffffff)
#define One (0x3f800000)
#define SpecialBound                                                           \
  (0x42af5e20) /* asuint(0x1.5ebc4p+6). Largest value of x for which expm1(x)  \
		  should round to -1.  */
#define TinyBound (0x34000000) /* asuint(0x1p-23).  */

#define C(i) v_f32 (__expm1f_poly[i])

/* Single-precision vector exp(x) - 1 function.
   The maximum error is 1.51 ULP:
   expm1f(0x1.8baa96p-2) got 0x1.e2fb9p-2
			want 0x1.e2fb94p-2.  */
VPCS_ATTR
float32x4_t V_NAME_F1 (expm1) (float32x4_t x)
{
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t ax = ix & AbsMask;

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, fall back to the scalar
     variant for all lanes if any of them should trigger an exception.  */
  uint32x4_t special
    = (ax >= SpecialBound) | (ix == 0x80000000) | (ax < TinyBound);
  if (unlikely (v_any_u32 (special)))
    return v_call_f32 (expm1f, x, x, v_u32 (0xffffffff));
#else
  /* Handles very large values (+ve and -ve), +/-NaN, +/-Inf and -0.  */
  uint32x4_t special = (ax >= SpecialBound) | (ix == 0x80000000);
#endif

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  float32x4_t j = vfmaq_f32 (Shift, InvLn2, x) - Shift;
  int32x4_t i = vcvtq_s32_f32 (j);
  float32x4_t f = vfmaq_f32 (x, j, MLn2hi);
  f = vfmaq_f32 (f, j, MLn2lo);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */

  float32x4_t p = vfmaq_f32 (C (3), C (4), f);
  p = vfmaq_f32 (C (2), p, f);
  p = vfmaq_f32 (C (1), p, f);
  p = vfmaq_f32 (C (0), p, f);
  p = vfmaq_f32 (f, f * f, p);

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  float32x4_t t = vreinterpretq_f32_u32 (vreinterpretq_u32_s32 (i << 23) + One);
  /* expm1(x) ~= p * t + (t - 1).  */
  float32x4_t y = vfmaq_f32 (t - 1, p, t);

#if !WANT_SIMD_EXCEPT
  if (unlikely (v_any_u32 (special)))
    return v_call_f32 (expm1f, x, y, special);
#endif

  return y;
}

PL_SIG (V, F, 1, expm1, -9.9, 9.9)
PL_TEST_ULP (V_NAME_F1 (expm1), 1.02)
PL_TEST_EXPECT_FENV (V_NAME_F1 (expm1), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME_F1 (expm1), 0, 0x1p-23, 1000)
PL_TEST_INTERVAL (V_NAME_F1 (expm1), -0, -0x1p-23, 1000)
PL_TEST_INTERVAL (V_NAME_F1 (expm1), 0x1p-23, 0x1.644716p6, 1000000)
PL_TEST_INTERVAL (V_NAME_F1 (expm1), -0x1p-23, -0x1.9bbabcp+6, 1000000)
