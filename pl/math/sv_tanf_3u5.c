/*
 * Single-precision vector tan(x) function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "sv_estrinf.h"

#if SV_SUPPORTED

/* Constants.  */
#define NegPio2_1 (sv_f32 (-0x1.921fb6p+0f))
#define NegPio2_2 (sv_f32 (0x1.777a5cp-25f))
#define NegPio2_3 (sv_f32 (0x1.ee59dap-50f))
#define InvPio2 (sv_f32 (0x1.45f306p-1f))
#define RangeVal (sv_f32 (0x1p15f))
#define Shift (sv_f32 (0x1.8p+23f))

#define poly(i) sv_f32 (__tanf_poly_data.poly_tan[i])

/* Use full Estrin's scheme to evaluate polynomial.  */
static inline svfloat32_t
eval_poly (svbool_t pg, svfloat32_t z)
{
  svfloat32_t z2 = svmul_f32_x (pg, z, z);
  svfloat32_t z4 = svmul_f32_x (pg, z2, z2);
  return ESTRIN_5 (pg, z, z2, z4, poly);
}

static NOINLINE svfloat32_t
__sv_tanf_specialcase (svfloat32_t x, svfloat32_t y, svbool_t cmp)
{
  return sv_call_f32 (tanf, x, y, cmp);
}

/* Fast implementation of SVE tanf.
   Maximum error is 3.45 ULP:
   SV_NAME_F1 (tan)(-0x1.e5f0cap+13) got 0x1.ff9856p-1
			     want 0x1.ff9850p-1.  */
svfloat32_t SV_NAME_F1 (tan) (svfloat32_t x, const svbool_t pg)
{
  /* Determine whether input is too large to perform fast regression.  */
  svbool_t cmp = svacge_f32 (pg, x, RangeVal);
  svbool_t pred_minuszero = svcmpeq_f32 (pg, x, sv_f32 (-0.0));

  /* n = rint(x/(pi/2)).  */
  svfloat32_t q = svmla_f32_x (pg, Shift, x, InvPio2);
  svfloat32_t n = svsub_f32_x (pg, q, Shift);
  /* n is already a signed integer, simply convert it.  */
  svint32_t in = svcvt_s32_f32_x (pg, n);
  /* Determine if x lives in an interval, where |tan(x)| grows to infinity.  */
  svint32_t alt = svand_s32_x (pg, in, sv_s32 (1));
  svbool_t pred_alt = svcmpne_s32 (pg, alt, sv_s32 (0));

  /* r = x - n * (pi/2)  (range reduction into 0 .. pi/4).  */
  svfloat32_t r;
  r = svmla_f32_x (pg, x, n, NegPio2_1);
  r = svmla_f32_x (pg, r, n, NegPio2_2);
  r = svmla_f32_x (pg, r, n, NegPio2_3);

  /* If x lives in an interval, where |tan(x)|
     - is finite, then use a polynomial approximation of the form
       tan(r) ~ r + r^3 * P(r^2) = r + r * r^2 * P(r^2).
     - grows to infinity then use symmetries of tangent and the identity
       tan(r) = cotan(pi/2 - r) to express tan(x) as 1/tan(-r). Finally, use
       the same polynomial approximation of tan as above.  */

  /* Perform additional reduction if required.  */
  svfloat32_t z = svneg_f32_m (r, pred_alt, r);

  /* Evaluate polynomial approximation of tangent on [-pi/4, pi/4].  */
  svfloat32_t z2 = svmul_f32_x (pg, z, z);
  svfloat32_t p = eval_poly (pg, z2);
  svfloat32_t y = svmla_f32_x (pg, z, p, svmul_f32_x (pg, z, z2));

  /* Transform result back, if necessary.  */
  svfloat32_t inv_y = svdiv_f32_x (pg, sv_f32 (1.0f), y);
  y = svsel_f32 (pred_alt, inv_y, y);

  /* Fast reduction does not handle the x = -0.0 case well,
     therefore it is fixed here.  */
  y = svsel_f32 (pred_minuszero, x, y);

  /* No need to pass pg to specialcase here since cmp is a strict subset,
     guaranteed by the cmpge above.  */
  if (unlikely (svptest_any (pg, cmp)))
    return __sv_tanf_specialcase (x, y, cmp);
  return y;
}

PL_SIG (SV, F, 1, tan, -3.1, 3.1)
PL_TEST_ULP (SV_NAME_F1 (tan), 2.96)
PL_TEST_INTERVAL (SV_NAME_F1 (tan), -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (SV_NAME_F1 (tan), 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (SV_NAME_F1 (tan), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (tan), 0x1p-23, 0.7, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (tan), 0.7, 1.5, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (tan), 1.5, 100, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (tan), 100, 0x1p17, 50000)
PL_TEST_INTERVAL (SV_NAME_F1 (tan), 0x1p17, inf, 50000)
#endif
