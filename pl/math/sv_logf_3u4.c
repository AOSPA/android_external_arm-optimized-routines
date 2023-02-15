/*
 * Single-precision vector log function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define P(i) __sv_logf_poly[i]

#define Ln2 (0x1.62e43p-1f) /* 0x3f317218 */
#define Min (0x00800000)
#define Max (0x7f800000)
#define Mask (0x007fffff)
#define Off (0x3f2aaaab) /* 0.666667 */

float
optr_aor_log_f32 (float);

static NOINLINE svfloat32_t
__sv_logf_specialcase (svfloat32_t x, svfloat32_t y, svbool_t cmp)
{
  return sv_call_f32 (optr_aor_log_f32, x, y, cmp);
}

/* Optimised implementation of SVE logf, using the same algorithm and polynomial
   as the Neon routine in math/. Maximum error is 3.34 ULPs:
   SV_NAME_F1 (log)(0x1.557298p+0) got 0x1.26edecp-2
			   want 0x1.26ede6p-2.  */
svfloat32_t SV_NAME_F1 (log) (svfloat32_t x, const svbool_t pg)
{
  svuint32_t u = svreinterpret_u32_f32 (x);
  svbool_t cmp
    = svcmpge_u32 (pg, svsub_n_u32_x (pg, u, Min), sv_u32 (Max - Min));

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u = svsub_n_u32_x (pg, u, Off);
  svfloat32_t n
    = svcvt_f32_s32_x (pg, svasr_n_s32_x (pg, svreinterpret_s32_u32 (u),
					  23)); /* Sign-extend.  */
  u = svand_n_u32_x (pg, u, Mask);
  u = svadd_n_u32_x (pg, u, Off);
  svfloat32_t r = svsub_n_f32_x (pg, svreinterpret_f32_u32 (u), 1.0f);

  /* y = log(1+r) + n*ln2.  */
  svfloat32_t r2 = svmul_f32_x (pg, r, r);
  /* n*ln2 + r + r2*(P6 + r*P5 + r2*(P4 + r*P3 + r2*(P2 + r*P1 + r2*P0))).  */
  svfloat32_t p = svmla_n_f32_x (pg, sv_f32 (P (2)), r, P (1));
  svfloat32_t q = svmla_n_f32_x (pg, sv_f32 (P (4)), r, P (3));
  svfloat32_t y = svmla_n_f32_x (pg, sv_f32 (P (6)), r, P (5));
  p = svmla_n_f32_x (pg, p, r2, P (0));
  q = svmla_f32_x (pg, q, r2, p);
  y = svmla_f32_x (pg, y, r2, q);
  p = svmla_n_f32_x (pg, r, n, Ln2);
  y = svmla_f32_x (pg, p, r2, y);

  if (unlikely (svptest_any (pg, cmp)))
    return __sv_logf_specialcase (x, y, cmp);
  return y;
}

PL_SIG (SV, F, 1, log, 0.01, 11.1)
PL_TEST_ULP (SV_NAME_F1 (log), 2.85)
PL_TEST_INTERVAL (SV_NAME_F1 (log), -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 1.0, 100, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log), 100, inf, 50000)
#endif // SV_SUPPORTED
