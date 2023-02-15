/*
 * Double-precision vector log10(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "include/mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#define A(i) v_f64 (__v_log10_data.poly[i])
#define T(s, i) __v_log10_data.tab[i].s
#define Ln2 v_f64 (0x1.62e42fefa39efp-1)
#define N (1 << V_LOG10_TABLE_BITS)
#define OFF v_u64 (0x3fe6900900000000)

#define BigBound 0x7ff0000000000000
#define TinyBound 0x0010000000000000

struct entry
{
  float64x2_t invc;
  float64x2_t log10c;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  e.invc[0] = T (invc, i[0]);
  e.log10c[0] = T (log10c, i[0]);
  e.invc[1] = T (invc, i[1]);
  e.log10c[1] = T (log10c, i[1]);
  return e;
}

VPCS_ATTR
inline static float64x2_t
specialcase (float64x2_t x, float64x2_t y, uint64x2_t cmp)
{
  return v_call_f64 (log10, x, y, cmp);
}

/* Our implementation of v_log10 is a slight modification of v_log (1.660ulps).
   Max ULP error: < 2.5 ulp (nearest rounding.)
   Maximum measured at 2.46 ulp for x in [0.96, 0.97]
     __v_log10(0x1.13192407fcb46p+0) got 0x1.fff6be3cae4bbp-6
				    want 0x1.fff6be3cae4b9p-6
     -0.459999 ulp err 1.96.  */
VPCS_ATTR
float64x2_t V_NAME_D1 (log10) (float64x2_t x)
{
  float64x2_t z, r, r2, p, y, kd, hi;
  uint64x2_t ix, iz, tmp, i, cmp;
  int64x2_t k;
  struct entry e;

  ix = vreinterpretq_u64_f64 (x);
  cmp = ix - TinyBound >= BigBound - TinyBound;

  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  tmp = ix - OFF;
  i = (tmp >> (52 - V_LOG10_TABLE_BITS)) % N;
  k = vreinterpretq_s64_u64 (tmp) >> 52; /* arithmetic shift.  */
  iz = ix - (tmp & v_u64 (0xfffULL << 52));
  z = vreinterpretq_f64_u64 (iz);
  e = lookup (i);

  /* log10(x) = log1p(z/c-1)/log(10) + log10(c) + k*log10(2).  */
  r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  kd = vcvtq_f64_s64 (k);

  /* hi = r / log(10) + log10(c) + k*log10(2).
     Constants in `v_log10_data.c` are computed (in extended precision) as
     e.log10c := e.logc * ivln10.  */
  float64x2_t w = vfmaq_f64 (e.log10c, r, v_f64 (__v_log10_data.invln10));

  /* y = log10(1+r) + n * log10(2).  */
  hi = vfmaq_f64 (w, kd, v_f64 (__v_log10_data.log10_2));

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  r2 = r * r;
  y = vfmaq_f64 (A (2), A (3), r);
  p = vfmaq_f64 (A (0), A (1), r);
  y = vfmaq_f64 (y, A (4), r2);
  y = vfmaq_f64 (p, y, r2);
  y = vfmaq_f64 (hi, y, r2);

  if (unlikely (v_any_u64 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}

PL_SIG (V, D, 1, log10, 0.01, 11.1)
PL_TEST_ULP (V_NAME_D1 (log10), 1.97)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME_D1 (log10))
PL_TEST_INTERVAL (V_NAME_D1 (log10), 0, 0xffff000000000000, 10000)
PL_TEST_INTERVAL (V_NAME_D1 (log10), 0x1p-4, 0x1p4, 400000)
PL_TEST_INTERVAL (V_NAME_D1 (log10), 0, inf, 400000)
