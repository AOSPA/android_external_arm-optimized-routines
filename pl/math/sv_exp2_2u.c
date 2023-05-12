/*
 * Double-precision SVE 2^x function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "sv_estrin.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define N (1 << V_EXP_TABLE_BITS)

#define BigBound 1022
#define UOFlowBound 1280

static struct __sv_exp2_data
{
  double poly[4];
  double shift, uoflow_bound;
} data = {.shift = 0x1.8p52 / N,
	  .uoflow_bound = UOFlowBound,
	  /* Coefficients are reproducible using math/tools/exp2.sollya with
	     minimisation of the absolute error.  */
	  .poly = {0x1.62e42fefa3686p-1, 0x1.ebfbdff82c241p-3,
		   0x1.c6b09b16de99ap-5, 0x1.3b2abf5571ad8p-7}};

#define C(i) sv_f64 (data.poly[i])
#define SpecialOffset 0x6000000000000000 /* 0x1p513.  */
/* SpecialBias1 + SpecialBias1 = asuint(1.0).  */
#define SpecialBias1 0x7000000000000000 /* 0x1p769.  */
#define SpecialBias2 0x3010000000000000 /* 0x1p-254.  */

/* Update of both special and non-special cases, if any special case is
   detected.  */
static inline svfloat64_t
special_case (svbool_t pg, svfloat64_t s, svfloat64_t y, svfloat64_t n)
{
  /* s=2^n may overflow, break it up into s=s1*s2,
     such that exp = s + s*y can be computed as s1*(s2+s2*y)
     and s1*s1 overflows only if n>0.  */

  /* If n<=0 then set b to 0x6, 0 otherwise.  */
  svbool_t p_sign = svcmple_n_f64 (pg, n, 0.0); /* n <= 0.  */
  svuint64_t b = svdup_n_u64_z (p_sign, SpecialOffset);

  /* Set s1 to generate overflow depending on sign of exponent n.  */
  svfloat64_t s1 = svreinterpret_f64_u64 (svsubr_n_u64_x (pg, b, SpecialBias1));
  /* Offset s to avoid overflow in final result if n is below threshold.  */
  svfloat64_t s2 = svreinterpret_f64_u64 (svadd_u64_x (
    pg, svsub_n_u64_x (pg, svreinterpret_u64_f64 (s), SpecialBias2), b));

  /* |n| > 1280 => 2^(n) overflows.  */
  svbool_t p_cmp = svacgt_n_f64 (pg, n, data.uoflow_bound);

  svfloat64_t r1 = svmul_f64_x (pg, s1, s1);
  svfloat64_t r2 = svmla_f64_x (pg, s2, s2, y);
  svfloat64_t r0 = svmul_f64_x (pg, r2, s1);

  return svsel_f64 (p_cmp, r1, r0);
}

/* Fast vector implementation of exp2.
   Maximum measured error is 1.65 ulp.
   _ZGVsMxv_exp2(-0x1.4c264ab5b559bp-6) got 0x1.f8db0d4df721fp-1
				       want 0x1.f8db0d4df721dp-1.  */
svfloat64_t SV_NAME_D1 (exp2) (svfloat64_t x, svbool_t pg)
{
  svbool_t no_big_scale = svacle_n_f64 (pg, x, BigBound);
  svbool_t special = svnot_b_z (pg, no_big_scale);

  /* Reduce x to k/N + r, where k is integer and r in [-1/2N, 1/2N].  */
  svfloat64_t shift = sv_f64 (data.shift);
  svfloat64_t kd = svadd_f64_x (pg, x, shift);
  svuint64_t ki = svreinterpret_u64_f64 (kd);
  /* kd = k/N.  */
  kd = svsub_f64_x (pg, kd, shift);
  svfloat64_t r = svsub_f64_x (pg, x, kd);

  /* scale ~= 2^(k/N).  */
  svuint64_t idx = svand_n_u64_x (pg, ki, N - 1);
  svuint64_t sbits = svld1_gather_u64index_u64 (pg, __v_exp_data, idx);
  /* This is only a valid scale when -1023*N < k < 1024*N.  */
  svuint64_t top = svlsl_n_u64_x (pg, ki, 52 - V_EXP_TABLE_BITS);
  svfloat64_t scale = svreinterpret_f64_u64 (svadd_u64_x (pg, sbits, top));

  /* Approximate exp2(r) using polynomial.  */
  svfloat64_t r2 = svmul_f64_x (pg, r, r);
  svfloat64_t p = ESTRIN_3 (pg, r, r2, C);
  svfloat64_t y = svmul_f64_x (pg, r, p);

  /* Assemble exp2(x) = exp2(r) * scale.  */
  if (unlikely (svptest_any (pg, special)))
    return special_case (pg, scale, y, kd);
  return svmla_f64_x (pg, scale, scale, y);
}

PL_SIG (SV, D, 1, exp2, -9.9, 9.9)
PL_TEST_ULP (SV_NAME_D1 (exp2), 1.15)
PL_TEST_INTERVAL (SV_NAME_D1 (exp2), 0, BigBound, 1000)
PL_TEST_INTERVAL (SV_NAME_D1 (exp2), BigBound, UOFlowBound, 100000)
PL_TEST_INTERVAL (SV_NAME_D1 (exp2), UOFlowBound, inf, 1000)
PL_TEST_INTERVAL (SV_NAME_D1 (exp2), -0, -BigBound, 1000)
PL_TEST_INTERVAL (SV_NAME_D1 (exp2), -BigBound, -UOFlowBound, 100000)
PL_TEST_INTERVAL (SV_NAME_D1 (exp2), -UOFlowBound, -inf, 1000)
#endif
