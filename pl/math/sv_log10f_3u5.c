/*
 * Single-precision SVE log10 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "sv_estrinf.h"

#if SV_SUPPORTED

#define SpecialCaseMin 0x00800000
#define SpecialCaseMax 0x7f800000
#define Offset 0x3f2aaaab /* 0.666667.  */
#define Mask 0x007fffff
#define Ln2 0x1.62e43p-1f /* 0x3f317218.  */
#define InvLn10 0x1.bcb7b2p-2f

#define P(i) sv_f32 (__v_log10f_poly[i])

static NOINLINE svfloat32_t
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (log10f, x, y, special);
}

/* Optimised implementation of SVE log10f using the same algorithm and
   polynomial as v_log10f. Maximum error is 3.31ulps:
   SV_NAME_F1 (log10)(0x1.555c16p+0) got 0x1.ffe2fap-4
			     want 0x1.ffe2f4p-4.  */
svfloat32_t SV_NAME_F1 (log10) (svfloat32_t x, const svbool_t pg)
{
  svuint32_t ix = svreinterpret_u32_f32 (x);
  svbool_t special_cases
    = svcmpge_n_u32 (pg, svsub_n_u32_x (pg, ix, SpecialCaseMin),
		     SpecialCaseMax - SpecialCaseMin);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  ix = svsub_n_u32_x (pg, ix, Offset);
  svfloat32_t n
    = svcvt_f32_s32_x (pg, svasr_n_s32_x (pg, svreinterpret_s32_u32 (ix),
					  23)); /* signextend.  */
  ix = svand_n_u32_x (pg, ix, Mask);
  ix = svadd_n_u32_x (pg, ix, Offset);
  svfloat32_t r = svsub_n_f32_x (pg, svreinterpret_f32_u32 (ix), 1.0f);

  /* y = log10(1+r) + n*log10(2)
     log10(1+r) ~ r * InvLn(10) + P(r)
     where P(r) is a polynomial. Use order 9 for log10(1+x), i.e. order 8 for
     log10(1+x)/x, with x in [-1/3, 1/3] (offset=2/3) */
  svfloat32_t r2 = svmul_f32_x (pg, r, r);
  svfloat32_t r4 = svmul_f32_x (pg, r2, r2);
  svfloat32_t y = ESTRIN_7 (pg, r, r2, r4, P);

  /* Using p = Log10(2)*n + r*InvLn(10) is slightly faster but less
     accurate.  */
  svfloat32_t p = svmla_n_f32_x (pg, r, n, Ln2);
  y = svmla_f32_x (pg, svmul_n_f32_x (pg, p, InvLn10), r2, y);

  if (unlikely (svptest_any (pg, special_cases)))
    {
      return special_case (x, y, special_cases);
    }
  return y;
}

PL_SIG (SV, F, 1, log10, 0.01, 11.1)
PL_TEST_ULP (SV_NAME_F1 (log10), 2.82)
PL_TEST_INTERVAL (SV_NAME_F1 (log10), -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (SV_NAME_F1 (log10), 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (SV_NAME_F1 (log10), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log10), 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log10), 1.0, 100, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (log10), 100, inf, 50000)
#endif
