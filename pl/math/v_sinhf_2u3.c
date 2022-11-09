/*
 * Single-precision vector sinh(x) function.
 * Copyright (c) 2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "mathlib.h"

#if V_SUPPORTED

#define AbsMask 0x7fffffff
#define Half 0x3f000000
#define Expm1OFlowLimit                                                        \
  0x42b17218 /* 0x1.62e43p+6, 2^7*ln2, minimum value for which expm1f          \
		overflows.  */

/* Approximation for vector single-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The maximum error is 2.26 ULP:
   __v_sinhf(0x1.e34a9ep-4) got 0x1.e469ep-4 want 0x1.e469e4p-4.  */
VPCS_ATTR v_f32_t V_NAME (sinhf) (v_f32_t x)
{
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t iax = ix & AbsMask;
  v_f32_t ax = v_as_f32_u32 (iax);
  v_u32_t sign = ix & ~AbsMask;
  v_f32_t halfsign = v_as_f32_u32 (sign | Half);

  v_u32_t special = v_cond_u32 (iax >= Expm1OFlowLimit);
  /* Fall back to the scalar variant for all lanes if any of them should trigger
     an exception.  */
  if (unlikely (v_any_u32 (special)))
    return v_call_f32 (sinhf, x, x, v_u32 (-1));

  /* Up to the point that expm1f overflows, we can use it to calculate sinhf
     using a slight rearrangement of the definition of asinh. This allows us to
     retain acceptable accuracy for very small inputs.  */
  v_f32_t t = V_NAME (expm1f) (ax);
  return (t + t / (t + 1)) * halfsign;
}
VPCS_ALIAS

#endif
