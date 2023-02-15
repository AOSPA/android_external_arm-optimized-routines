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

#define One 0x3f800000
#define Shift v_f32 (0x1.8p23f)
#define InvLn2 v_f32 (0x1.715476p+0f)
#define MLn2hi v_f32 (-0x1.62e4p-1f)
#define MLn2lo v_f32 (-0x1.7f7d1cp-20f)

#define C(i) v_f32 (__expm1f_poly[i])

static inline float32x4_t
expm1f_inline (float32x4_t x)
{
  /* Helper routine for calculating exp(x) - 1.
     Copied from v_expm1f_1u6.c, with all special-case handling removed - the
     calling routine should handle special values if required.  */

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  float32x4_t j = vfmaq_f32 (Shift, InvLn2, x) - Shift;
  int32x4_t i = vcvtq_s32_f32 (j);
  float32x4_t f = vfmaq_f32 (x, j, MLn2hi);
  f = vfmaq_f32 (f, j, MLn2lo);

  /* Approximate expm1(f) with polynomial P, expm1(f) ~= f + f^2 * P(f).
     Uses Estrin scheme, where the main __v_expm1f routine uses Horner.  */
  float32x4_t f2 = f * f;
  float32x4_t p = ESTRIN_4 (f, f2, f2 * f2, C);
  p = vfmaq_f32 (f, f2, p);

  /* t = 2^i.  */
  float32x4_t t = vreinterpretq_f32_u32 (vreinterpretq_u32_s32 (i << 23) + One);
  /* expm1(x) ~= p * t + (t - 1).  */
  return vfmaq_f32 (t - 1, p, t);
}

#endif // PL_MATH_V_EXPM1F_INLINE_H
