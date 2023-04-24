/*
 * Helper for single-precision routines which calculate exp(x) - 1 and do not
 * need special-case handling
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_V_EXPM1F_INLINE_H
#define PL_MATH_V_EXPM1F_INLINE_H

#include "v_math.h"
#include "math_config.h"
#include "estrinf.h"

#define V_EXPM1F_INLINE_POLY_ORDER 4

struct v_expm1f_inline_data
{
  float32x4_t poly[V_EXPM1F_INLINE_POLY_ORDER + 1];
  float32x4_t invln2, ln2_lo, ln2_hi, shift;
};

static const volatile struct v_expm1f_inline_data data
  = {.invln2 = V4 (0x1.715476p+0f),
     .ln2_hi = V4 (0x1.62e4p-1f),
     .ln2_lo = V4 (0x1.7f7d1cp-20f),
     .shift = V4 (0x1.8p23f),
     /* Generated using fpminimax, see tools/expm1f.sollya for details.  */
     .poly = {V4 (0x1.fffffep-2), V4 (0x1.5554aep-3), V4 (0x1.555736p-5),
	      V4 (0x1.12287cp-7), V4 (0x1.6b55a2p-10)}};

#define ExponentBias v_s32 (0x3f800000) /* asuint(1.0f).  */
#define C(i) data.poly[i]

static inline float32x4_t
expm1f_inline (float32x4_t x)
{
  /* Helper routine for calculating exp(x) - 1.
     Copied from v_expm1f_1u6.c, with all special-case handling removed - the
     calling routine should handle special values if required.  */

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  float32x4_t j
    = vsubq_f32 (vfmaq_f32 (data.shift, data.invln2, x), data.shift);
  int32x4_t i = vcvtq_s32_f32 (j);
  float32x4_t f;
  f = vfmsq_f32 (x, j, data.ln2_hi);
  f = vfmsq_f32 (f, j, data.ln2_lo);

  /* Approximate expm1(f) with polynomial P, expm1(f) ~= f + f^2 * P(f).
     Uses Estrin scheme, where the main __v_expm1f routine uses Horner.  */
  float32x4_t f2 = vmulq_f32 (f, f);
  float32x4_t f4 = vmulq_f32 (f2, f2);
  float32x4_t p = ESTRIN_4 (f, f2, f4, C);
  p = vfmaq_f32 (f, f2, p);

  /* t = 2^i.  */
  int32x4_t u = vaddq_s32 (vshlq_n_s32 (i, 23), ExponentBias);
  float32x4_t t = vreinterpretq_f32_u32 (vreinterpretq_u32_s32 (u));
  /* expm1(x) ~= p * t + (t - 1).  */
  return vfmaq_f32 (vsubq_f32 (t, v_f32 (1.0f)), p, t);
}

#endif // PL_MATH_V_EXPM1F_INLINE_H
