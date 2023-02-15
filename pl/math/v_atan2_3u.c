/*
 * Double-precision vector atan2(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#include "atan_common.h"

#define PiOver2 v_f64 (0x1.921fb54442d18p+0)
#define SignMask v_u64 (0x8000000000000000)

/* Special cases i.e. 0, infinity, NaN (fall back to scalar calls).  */
VPCS_ATTR
NOINLINE static float64x2_t
specialcase (float64x2_t y, float64x2_t x, float64x2_t ret, uint64x2_t cmp)
{
  return v_call2_f64 (atan2, y, x, ret, cmp);
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline uint64x2_t
zeroinfnan (uint64x2_t i)
{
  return (2 * i - 1) >= v_u64 (2 * asuint64 (INFINITY) - 1);
}

/* Fast implementation of vector atan2.
   Maximum observed error is 2.8 ulps:
   v_atan2(0x1.9651a429a859ap+5, 0x1.953075f4ee26p+5)
	got 0x1.92d628ab678ccp-1
       want 0x1.92d628ab678cfp-1.  */
VPCS_ATTR
float64x2_t V_NAME_D2 (atan2) (float64x2_t y, float64x2_t x)
{
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t iy = vreinterpretq_u64_f64 (y);

  uint64x2_t special_cases = zeroinfnan (ix) | zeroinfnan (iy);

  uint64x2_t sign_x = ix & SignMask;
  uint64x2_t sign_y = iy & SignMask;
  uint64x2_t sign_xy = sign_x ^ sign_y;

  float64x2_t ax = vabsq_f64 (x);
  float64x2_t ay = vabsq_f64 (y);

  uint64x2_t pred_xlt0 = x < 0.0;
  uint64x2_t pred_aygtax = ay > ax;

  /* Set up z for call to atan.  */
  float64x2_t n = vbslq_f64 (pred_aygtax, -ax, ay);
  float64x2_t d = vbslq_f64 (pred_aygtax, ay, ax);
  float64x2_t z = vdivq_f64 (n, d);

  /* Work out the correct shift.  */
  float64x2_t shift = vbslq_f64 (pred_xlt0, v_f64 (-2.0), v_f64 (0.0));
  shift = vbslq_f64 (pred_aygtax, shift + 1.0, shift);
  shift *= PiOver2;

  float64x2_t ret = eval_poly (z, z, shift);

  /* Account for the sign of x and y.  */
  ret = vreinterpretq_f64_u64 (vreinterpretq_u64_f64 (ret) ^ sign_xy);

  if (unlikely (v_any_u64 (special_cases)))
    {
      return specialcase (y, x, ret, special_cases);
    }

  return ret;
}

/* Arity of 2 means no mathbench entry emitted. See test/mathbench_funcs.h.  */
PL_SIG (V, D, 2, atan2)
// TODO tighten this once __v_atan2 is fixed
PL_TEST_ULP (V_NAME_D2 (atan2), 2.9)
PL_TEST_INTERVAL (V_NAME_D2 (atan2), -10.0, 10.0, 50000)
PL_TEST_INTERVAL (V_NAME_D2 (atan2), -1.0, 1.0, 40000)
PL_TEST_INTERVAL (V_NAME_D2 (atan2), 0.0, 1.0, 40000)
PL_TEST_INTERVAL (V_NAME_D2 (atan2), 1.0, 100.0, 40000)
PL_TEST_INTERVAL (V_NAME_D2 (atan2), 1e6, 1e32, 40000)
