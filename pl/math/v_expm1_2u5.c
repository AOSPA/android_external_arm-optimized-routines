/*
 * Double-precision vector exp(x) - 1 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrin.h"
#include "pl_sig.h"
#include "pl_test.h"

#define V_EXPM1_POLY_ORDER 10

struct v_expm1_data
{
  float64x2_t poly[V_EXPM1_POLY_ORDER + 1];
  float64x2_t invln2, ln2_lo, ln2_hi, shift;
};

static const volatile struct v_expm1_data data
  = {.invln2 = V2 (0x1.71547652b82fep0),
     .ln2_hi = V2 (0x1.62e42fefa39efp-1),
     .ln2_lo = V2 (0x1.abc9e3b39803fp-56),
     .shift = V2 (0x1.8p52),
     /* Generated using fpminimax, see tools/expm1.sollya for details.  */
     .poly = {V2 (0x1p-1), V2 (0x1.5555555555559p-3), V2 (0x1.555555555554bp-5),
	      V2 (0x1.111111110f663p-7), V2 (0x1.6c16c16c1b5f3p-10),
	      V2 (0x1.a01a01affa35dp-13), V2 (0x1.a01a018b4ecbbp-16),
	      V2 (0x1.71ddf82db5bb4p-19), V2 (0x1.27e517fc0d54bp-22),
	      V2 (0x1.af5eedae67435p-26), V2 (0x1.1f143d060a28ap-29)}};

#define AllMask v_u64 (0xffffffffffffffff)
#define AbsMask v_u64 (0x7fffffffffffffff)
#define SignMask v_u64 (0x8000000000000000)
/* For |x| > BigBound, the final stage of the algorithm overflows so fall back
   to scalar.  */
#define BigBound 0x40862b7d369a5aa9    /* asuint64(0x1.62b7d369a5aa9p+9).  */
/* Value below which expm1(x) is within 2 ULP of x.  */
#define TinyBound 0x3cc0000000000000		/* asuint64(0x1p-51).  */
#define ExponentBias v_s64 (0x3ff0000000000000) /* asuint64(1.0).  */
#define C(i) data.poly[i]

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (expm1, x, y, special);
}

/* Double-precision vector exp(x) - 1 function.
   The maximum error observed error is 2.18 ULP:
   __v_expm1(0x1.634ba0c237d7bp-2) got 0x1.a8b9ea8d66e22p-2
				  want 0x1.a8b9ea8d66e2p-2.  */
float64x2_t VPCS_ATTR V_NAME_D1 (expm1) (float64x2_t x)
{
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t ax = vandq_u64 (ix, AbsMask);

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, fall back to the scalar
     variant for all lanes if any of them should trigger an exception.  */
  uint64x2_t special = vorrq_u64 (vcgeq_u64 (ax, v_u64 (BigBound)),
				  vcleq_u64 (ax, v_u64 (TinyBound)));
  if (unlikely (v_any_u64 (special)))
    return special_case (x, x, AllMask);
#else
  /* Large input, NaNs and Infs, or -0.  */
  uint64x2_t special
    = vorrq_u64 (vcgeq_u64 (ax, v_u64 (BigBound)), vceqq_u64 (ix, SignMask));
#endif

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  float64x2_t n
    = vsubq_f64 (vfmaq_f64 (data.shift, data.invln2, x), data.shift);
  int64x2_t i = vcvtq_s64_f64 (n);
  float64x2_t f;
  f = vfmsq_f64 (x, n, data.ln2_hi);
  f = vfmsq_f64 (f, n, data.ln2_lo);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  float64x2_t f2 = vmulq_f64 (f, f);
  float64x2_t f4 = vmulq_f64 (f2, f2);
  float64x2_t f8 = vmulq_f64 (f4, f4);
  float64x2_t p = vfmaq_f64 (f, f2, ESTRIN_10 (f, f2, f4, f8, C));

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  int64x2_t u = vaddq_s64 (vshlq_n_s64 (i, 52), ExponentBias);
  float64x2_t t = vreinterpretq_f64_s64 (u);
  /* expm1(x) ~= p * t + (t - 1).  */
  float64x2_t y = vfmaq_f64 (vsubq_f64 (t, v_f64 (1.0)), p, t);

#if !WANT_SIMD_EXCEPT
  if (unlikely (v_any_u64 (special)))
    return special_case (x, y, special);
#endif

  return y;
}

PL_SIG (V, D, 1, expm1, -9.9, 9.9)
PL_TEST_ULP (V_NAME_D1 (expm1), 1.68)
PL_TEST_EXPECT_FENV (V_NAME_D1 (expm1), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME_D1 (expm1), 0, TinyBound, 1000)
PL_TEST_INTERVAL (V_NAME_D1 (expm1), -0, -TinyBound, 1000)
PL_TEST_INTERVAL (V_NAME_D1 (expm1), TinyBound, BigBound, 100000)
PL_TEST_INTERVAL (V_NAME_D1 (expm1), -TinyBound, -BigBound, 100000)
PL_TEST_INTERVAL (V_NAME_D1 (expm1), BigBound, inf, 100)
PL_TEST_INTERVAL (V_NAME_D1 (expm1), -BigBound, -inf, 100)
