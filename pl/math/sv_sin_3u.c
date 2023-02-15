/*
 * Double-precision SVE sin(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define InvPi (sv_f64 (0x1.45f306dc9c883p-2))
#define HalfPi (sv_f64 (0x1.921fb54442d18p+0))
#define InvPio2 (sv_f64 (0x1.45f306dc9c882p-1))
#define NegPio2_1 (sv_f64 (-0x1.921fb50000000p+0))
#define NegPio2_2 (sv_f64 (-0x1.110b460000000p-26))
#define NegPio2_3 (sv_f64 (-0x1.1a62633145c07p-54))
#define Shift (sv_f64 (0x1.8p52))
#define RangeVal (sv_f64 (0x1p23))
#define AbsMask (0x7fffffffffffffff)

static NOINLINE svfloat64_t
__sv_sin_specialcase (svfloat64_t x, svfloat64_t y, svbool_t cmp)
{
  return sv_call_f64 (sin, x, y, cmp);
}

/* A fast SVE implementation of sin based on trigonometric
   instructions (FTMAD, FTSSEL, FTSMUL).
   Maximum observed error in 2.52 ULP:
   SV_NAME_D1 (sin)(0x1.2d2b00df69661p+19) got 0x1.10ace8f3e786bp-40
				  want 0x1.10ace8f3e7868p-40.  */
svfloat64_t SV_NAME_D1 (sin) (svfloat64_t x, const svbool_t pg)
{
  svfloat64_t n, r, r2, y;
  svuint64_t sign;
  svbool_t cmp;

  r = svreinterpret_f64_u64 (
    svand_n_u64_x (pg, svreinterpret_u64_f64 (x), AbsMask));
  sign = svand_n_u64_x (pg, svreinterpret_u64_f64 (x), ~AbsMask);
  cmp = svcmpge_u64 (pg, svreinterpret_u64_f64 (r),
		     svreinterpret_u64_f64 (RangeVal));

  /* n = rint(|x|/(pi/2)).  */
  svfloat64_t q = svmla_f64_x (pg, Shift, r, InvPio2);
  n = svsub_f64_x (pg, q, Shift);

  /* r = |x| - n*(pi/2)  (range reduction into -pi/4 .. pi/4).  */
  r = svmla_f64_x (pg, r, n, NegPio2_1);
  r = svmla_f64_x (pg, r, n, NegPio2_2);
  r = svmla_f64_x (pg, r, n, NegPio2_3);

  /* Final multiplicative factor: 1.0 or x depending on bit #0 of q.  */
  svfloat64_t f = svtssel_f64 (r, svreinterpret_u64_f64 (q));

  /* sin(r) poly approx.  */
  r2 = svtsmul_f64 (r, svreinterpret_u64_f64 (q));
  y = sv_f64 (0.0);
  y = svtmad_f64 (y, r2, 7);
  y = svtmad_f64 (y, r2, 6);
  y = svtmad_f64 (y, r2, 5);
  y = svtmad_f64 (y, r2, 4);
  y = svtmad_f64 (y, r2, 3);
  y = svtmad_f64 (y, r2, 2);
  y = svtmad_f64 (y, r2, 1);
  y = svtmad_f64 (y, r2, 0);

  /* Apply factor.  */
  y = svmul_f64_x (pg, f, y);

  /* sign = y^sign.  */
  y = svreinterpret_f64_u64 (sveor_u64_x (pg, svreinterpret_u64_f64 (y), sign));

  /* No need to pass pg to specialcase here since cmp is a strict subset,
     guaranteed by the cmpge above.  */
  if (unlikely (svptest_any (pg, cmp)))
    return __sv_sin_specialcase (x, y, cmp);
  return y;
}

PL_SIG (SV, D, 1, sin, -3.1, 3.1)
PL_TEST_ULP (SV_NAME_D1 (sin), 2.03)
PL_TEST_INTERVAL (SV_NAME_D1 (sin), 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (SV_NAME_D1 (sin), 0x1p-4, 0x1p4, 500000)
#endif
