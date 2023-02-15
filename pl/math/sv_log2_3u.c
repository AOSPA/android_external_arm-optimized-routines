/*
 * Double-precision SVE log2 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define InvLn2 sv_f64 (0x1.71547652b82fep0)
#define N (1 << V_LOG2_TABLE_BITS)
#define OFF 0x3fe6900900000000
#define P(i) sv_f64 (__v_log2_data.poly[i])

NOINLINE static svfloat64_t
specialcase (svfloat64_t x, svfloat64_t y, const svbool_t cmp)
{
  return sv_call_f64 (log2, x, y, cmp);
}

/* Double-precision SVE log2 routine. Implements the same algorithm as vector
   log10, with coefficients and table entries scaled in extended precision.
   The maximum observed error is 2.58 ULP:
   __v_log2(0x1.0b556b093869bp+0) got 0x1.fffb34198d9dap-5
				 want 0x1.fffb34198d9ddp-5.  */
svfloat64_t SV_NAME_D1 (log2) (svfloat64_t x, const svbool_t pg)
{
  svuint64_t ix = svreinterpret_u64_f64 (x);
  svuint64_t top = svlsr_n_u64_x (pg, ix, 48);

  svbool_t special
    = svcmpge_n_u64 (pg, svsub_n_u64_x (pg, top, 0x0010), 0x7ff0 - 0x0010);

  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_n_u64_x (pg, ix, OFF);
  svuint64_t i
    = sv_mod_n_u64_x (pg, svlsr_n_u64_x (pg, tmp, 52 - V_LOG2_TABLE_BITS), N);
  svfloat64_t k
    = svcvt_f64_s64_x (pg, svasr_n_s64_x (pg, svreinterpret_s64_u64 (tmp), 52));
  svfloat64_t z = svreinterpret_f64_u64 (
    svsub_u64_x (pg, ix, svand_n_u64_x (pg, tmp, 0xfffULL << 52)));

  svuint64_t idx = svmul_n_u64_x (pg, i, 2);
  svfloat64_t invc
    = svld1_gather_u64index_f64 (pg, &__v_log2_data.tab[0].invc, idx);
  svfloat64_t log2c
    = svld1_gather_u64index_f64 (pg, &__v_log2_data.tab[0].log2c, idx);

  /* log2(x) = log1p(z/c-1)/log(2) + log2(c) + k.  */

  svfloat64_t r = svmla_f64_x (pg, sv_f64 (-1.0), invc, z);
  svfloat64_t w = svmla_f64_x (pg, log2c, InvLn2, r);

  svfloat64_t r2 = svmul_f64_x (pg, r, r);
  svfloat64_t p_23 = svmla_f64_x (pg, P (2), r, P (3));
  svfloat64_t p_01 = svmla_f64_x (pg, P (0), r, P (1));
  svfloat64_t y = svmla_f64_x (pg, p_23, r2, P (4));
  y = svmla_f64_x (pg, p_01, r2, y);
  y = svmla_f64_x (pg, svadd_f64_x (pg, k, w), r2, y);

  if (unlikely (svptest_any (pg, special)))
    {
      return specialcase (x, y, special);
    }
  return y;
}

PL_SIG (SV, D, 1, log2, 0.01, 11.1)
PL_TEST_ULP (SV_NAME_D1 (log2), 2.09)
PL_TEST_EXPECT_FENV_ALWAYS (SV_NAME_D1 (log2))
PL_TEST_INTERVAL (SV_NAME_D1 (log2), -0.0, -0x1p126, 1000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 0.0, 0x1p-126, 4000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 1.0, 100, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (log2), 100, inf, 50000)

#endif
