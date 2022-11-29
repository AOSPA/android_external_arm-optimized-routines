/*
 * AdvSIMD vector PCS variant of __v_asinh.
 *
 * Copyright (c) 2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_asinh, _ZGVnN2v_asinh)
#include "v_asinh_2u5.c"
#endif
