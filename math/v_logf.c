/*
 * Single-precision vector log function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"

static const float Poly[] = {
  /* 3.34 ulp error */
  -0x1.3e737cp-3f, 0x1.5a9aa2p-3f, -0x1.4f9934p-3f, 0x1.961348p-3f,
  -0x1.00187cp-2f, 0x1.555d7cp-2f, -0x1.ffffc8p-2f,
};
#define P7 v_f32 (Poly[0])
#define P6 v_f32 (Poly[1])
#define P5 v_f32 (Poly[2])
#define P4 v_f32 (Poly[3])
#define P3 v_f32 (Poly[4])
#define P2 v_f32 (Poly[5])
#define P1 v_f32 (Poly[6])

#define Ln2 v_f32 (0x1.62e43p-1f) /* 0x3f317218 */
#define Min v_u32 (0x00800000)
#define Max v_u32 (0x7f800000)
#define Mask v_u32 (0x007fffff)
#define Off v_u32 (0x3f2aaaab) /* 0.666667 */

static float32x4_t VPCS_ATTR NOINLINE
specialcase (float32x4_t x, float32x4_t y, uint32x4_t cmp)
{
  /* Fall back to scalar code.  */
  return v_call_f32 (logf, x, y, cmp);
}

float32x4_t VPCS_ATTR V_NAME_F1 (log) (float32x4_t x)
{
  float32x4_t n, p, q, r, r2, y;
  uint32x4_t u, cmp;

  u = vreinterpretq_u32_f32 (x);
  cmp = u - Min >= Max - Min;

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3 */
  u -= Off;
  n = v_to_f32_s32 (vreinterpretq_s32_u32 (u) >> 23); /* signextend.  */
  u &= Mask;
  u += Off;
  r = vreinterpretq_f32_u32 (u) - v_f32 (1.0f);

  /* y = log(1+r) + n*ln2.  */
  r2 = r * r;
  /* n*ln2 + r + r2*(P1 + r*P2 + r2*(P3 + r*P4 + r2*(P5 + r*P6 + r2*P7))).  */
  p = vfmaq_f32 (P5, P6, r);
  q = vfmaq_f32 (P3, P4, r);
  y = vfmaq_f32 (P1, P2, r);
  p = vfmaq_f32 (p, P7, r2);
  q = vfmaq_f32 (q, p, r2);
  y = vfmaq_f32 (y, q, r2);
  p = vfmaq_f32 (r, Ln2, n);
  y = vfmaq_f32 (p, y, r2);

  if (unlikely (v_any_u32 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}
