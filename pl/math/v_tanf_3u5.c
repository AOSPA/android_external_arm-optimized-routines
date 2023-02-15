/*
 * Single-precision vector tan(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrinf.h"
#include "pl_sig.h"
#include "pl_test.h"

/* Constants.  */
#define NegPio2_1 (v_f32 (-0x1.921fb6p+0f))
#define NegPio2_2 (v_f32 (0x1.777a5cp-25f))
#define NegPio2_3 (v_f32 (0x1.ee59dap-50f))
#define InvPio2 (v_f32 (0x1.45f306p-1f))
#define RangeVal (0x47000000)  /* asuint32(0x1p15f).  */
#define TinyBound (0x30000000) /* asuint32 (0x1p-31).  */
#define Shift (v_f32 (0x1.8p+23f))
#define AbsMask (v_u32 (0x7fffffff))

#define poly(i) v_f32 (__tanf_poly_data.poly_tan[i])

/* Special cases (fall back to scalar calls).  */
VPCS_ATTR
NOINLINE static float32x4_t
specialcase (float32x4_t x, float32x4_t y, uint32x4_t cmp)
{
  return v_call_f32 (tanf, x, y, cmp);
}

/* Use a full Estrin scheme to evaluate polynomial.  */
static inline float32x4_t
eval_poly (float32x4_t z)
{
  float32x4_t z2 = z * z;
#if WANT_SIMD_EXCEPT
  /* Tiny z (<= 0x1p-31) will underflow when calculating z^4. If fp exceptions
     are to be triggered correctly, sidestep this by fixing such lanes to 0.  */
  uint32x4_t will_uflow = (vreinterpretq_u32_f32 (z) & AbsMask) <= TinyBound;
  if (unlikely (v_any_u32 (will_uflow)))
    z2 = vbslq_f32 (will_uflow, v_f32 (0), z2);
#endif
  float32x4_t z4 = z2 * z2;
  return ESTRIN_5 (z, z2, z4, poly);
}

/* Fast implementation of Neon tanf.
   Maximum error is 3.45 ULP:
   __v_tanf(-0x1.e5f0cap+13) got 0x1.ff9856p-1
			    want 0x1.ff9850p-1.  */
VPCS_ATTR
float32x4_t V_NAME_F1 (tan) (float32x4_t x)
{
  float32x4_t special_arg = x;
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t iax = ix & AbsMask;

  /* iax >= RangeVal means x, if not inf or NaN, is too large to perform fast
     regression.  */
#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, also special-case tiny
     input, as this will load to overflow later. Fix any special lanes to 1 to
     prevent any exceptions being triggered.  */
  uint32x4_t special = iax - TinyBound >= RangeVal - TinyBound;
  if (unlikely (v_any_u32 (special)))
    x = vbslq_f32 (special, v_f32 (1.0f), x);
#else
  /* Otherwise, special-case large and special values.  */
  uint32x4_t special = iax >= RangeVal;
#endif

  /* n = rint(x/(pi/2)).  */
  float32x4_t q = vfmaq_f32 (Shift, InvPio2, x);
  float32x4_t n = q - Shift;
  /* n is representable as a signed integer, simply convert it.  */
  int32x4_t in = vcvtaq_s32_f32 (n);
  /* Determine if x lives in an interval, where |tan(x)| grows to infinity.  */
  int32x4_t alt = in & 1;
  uint32x4_t pred_alt = (alt != 0);

  /* r = x - n * (pi/2)  (range reduction into -pi./4 .. pi/4).  */
  float32x4_t r;
  r = vfmaq_f32 (x, NegPio2_1, n);
  r = vfmaq_f32 (r, NegPio2_2, n);
  r = vfmaq_f32 (r, NegPio2_3, n);

  /* If x lives in an interval, where |tan(x)|
     - is finite, then use a polynomial approximation of the form
       tan(r) ~ r + r^3 * P(r^2) = r + r * r^2 * P(r^2).
     - grows to infinity then use symmetries of tangent and the identity
       tan(r) = cotan(pi/2 - r) to express tan(x) as 1/tan(-r). Finally, use
       the same polynomial approximation of tan as above.  */

  /* Perform additional reduction if required.  */
  float32x4_t z = vbslq_f32 (pred_alt, -r, r);

  /* Evaluate polynomial approximation of tangent on [-pi/4, pi/4].  */
  float32x4_t z2 = r * r;
  float32x4_t p = eval_poly (z2);
  float32x4_t y = vfmaq_f32 (z, z * z2, p);

  /* Compute reciprocal and apply if required.  */
  float32x4_t inv_y = vdivq_f32 (v_f32 (1.0f), y);
  y = vbslq_f32 (pred_alt, inv_y, y);

  /* Fast reduction does not handle the x = -0.0 case well,
     therefore it is fixed here.  */
  y = vbslq_f32 (x == v_f32 (-0.0), x, y);

  if (unlikely (v_any_u32 (special)))
    return specialcase (special_arg, y, special);
  return y;
}

PL_SIG (V, F, 1, tan, -3.1, 3.1)
PL_TEST_ULP (V_NAME_F1 (tan), 2.96)
PL_TEST_EXPECT_FENV (V_NAME_F1 (tan), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME_F1 (tan), -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (V_NAME_F1 (tan), 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (V_NAME_F1 (tan), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (tan), 0x1p-23, 0.7, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (tan), 0.7, 1.5, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (tan), 1.5, 100, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (tan), 100, 0x1p17, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (tan), 0x1p17, inf, 50000)
