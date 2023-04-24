/*
 * Single-precision vector e^x function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "sv_estrinf.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "sv_expf_specialcase.h"

#if SV_SUPPORTED

#define SV_EXPF_POLY_ORDER 4
struct __sv_expf_data
{
  float poly[SV_EXPF_POLY_ORDER + 1];
  float inv_ln2, ln2_hi, ln2_lo, shift, thres;
};

static struct __sv_expf_data data = {
  /* Coefficients copied from the polynomial in math/v_expf.c, reversed for
     compatibility with polynomial helpers.  */
  .poly = {0x1.ffffecp-1f, 0x1.fffdb6p-2f, 0x1.555e66p-3f, 0x1.573e2ep-5f,
	   0x1.0e4020p-7f},

  .inv_ln2 = 0x1.715476p+0f,
  .ln2_hi = 0x1.62e4p-1f,
  .ln2_lo = 0x1.7f7d1cp-20f,

#if SV_EXPF_USE_FEXPA
  /* 1.5*2^17 + 127.  */
  .shift = 0x1.903f8p17f,
  /* Roughly 87.3. For x < -Thres, the result is subnormal and not handled
     correctly by FEXPA.  */
  .thres = 0x1.5d5e2ap+6f,
#else
  /* 1.5 * 2^23.  */
  .shift = 0x1.8p23f,
  .thres = 126.0f,
#endif
};

#define C(i) sv_f32 (data.poly[i])
#define ExponentBias 0x3f800000

#if SV_EXPF_USE_FEXPA

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  /* The special-case handler from the Neon routine does not handle subnormals
     in a way that is compatible with FEXPA. For the FEXPA variant we just fall
     back to scalar expf.  */
  return sv_call_f32 (expf, x, y, special);
}

#endif

/* Optimised single-precision SVE exp function. By default this is an SVE port
   of the Neon algorithm from math/. Alternatively, enable a modification of
   that algorithm that looks up scale using SVE FEXPA instruction with
   SV_EXPF_USE_FEXPA.

   Worst-case error of the default algorithm is 1.95 ulp:
   SV_NAME_F1 (exp)(-0x1.4cb74ap+2) got 0x1.6a022cp-8
				   want 0x1.6a023p-8.

   Worst-case error when using FEXPA is 1.04 ulp:
   SV_NAME_F1 (exp)(0x1.a8eda4p+1) got 0x1.ba74bcp+4
				  want 0x1.ba74bap+4.  */
svfloat32_t SV_NAME_F1 (exp) (svfloat32_t x, const svbool_t pg)
{
  /* exp(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = ln2*n + r, with r in [-ln2/2, ln2/2].  */

  /* Load some constants in quad-word chunks to minimise memory access (last
     lane is wasted).  */
  svfloat32_t invln2_and_ln2 = svld1rq_f32 (pg, &data.inv_ln2);

  /* n = round(x/(ln2/N)).  */
  svfloat32_t z = svmla_lane_f32 (sv_f32 (data.shift), x, invln2_and_ln2, 0);
  svfloat32_t n = svsub_n_f32_x (pg, z, data.shift);

  /* r = x - n*ln2/N.  */
  svfloat32_t r = svmls_lane_f32 (x, n, invln2_and_ln2, 1);
  r = svmls_lane_f32 (r, n, invln2_and_ln2, 2);

/* scale = 2^(n/N).  */
#if SV_EXPF_USE_FEXPA
  /* NaNs also need special handling with FEXPA.  */
  svbool_t is_special_case
    = svorr_b_z (pg, svacgt_n_f32 (pg, x, data.thres), svcmpne_f32 (pg, x, x));
  svfloat32_t scale = svexpa_f32 (svreinterpret_u32_f32 (z));
#else
  svuint32_t e = svlsl_n_u32_x (pg, svreinterpret_u32_f32 (z), 23);
  svbool_t is_special_case = svacgt_n_f32 (pg, n, data.thres);
  svfloat32_t scale
    = svreinterpret_f32_u32 (svadd_n_u32_x (pg, e, ExponentBias));
#endif

  /* y = exp(r) - 1 ~= r + C0 r^2 + C1 r^3 + C2 r^4 + C3 r^5 + C4 r^6.  */
  svfloat32_t r2 = svmul_f32_x (pg, r, r);
  /* Evaluate polynomial use hybrid scheme - offset variant of ESTRIN macro for
     coefficients 1 to 4, and apply most significant coefficient directly.  */
  svfloat32_t p14 = ESTRIN_3_ (pg, r, r2, C, 1);
  svfloat32_t p0 = svmul_f32_x (pg, r, C (0));
  svfloat32_t poly = svmla_f32_x (pg, p0, r2, p14);

  if (unlikely (svptest_any (pg, is_special_case)))
#if SV_EXPF_USE_FEXPA
    return special_case (x, svmla_f32_x (pg, scale, scale, poly),
			 is_special_case);
#else
    return __sv_expf_specialcase (pg, poly, n, e, is_special_case, scale);
#endif

  return svmla_f32_x (pg, scale, scale, poly);
}

PL_SIG (SV, F, 1, exp, -9.9, 9.9)
PL_TEST_ULP (SV_NAME_F1 (exp), 1.46)
PL_TEST_INTERVAL (SV_NAME_F1 (exp), 0, 0x1p-23, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp), 0x1p-23, 1, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp), 1, 0x1p23, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp), 0x1p23, inf, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp), -0, -0x1p-23, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp), -0x1p-23, -1, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp), -1, -0x1p23, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (exp), -0x1p23, -inf, 50000)
#endif // SV_SUPPORTED
