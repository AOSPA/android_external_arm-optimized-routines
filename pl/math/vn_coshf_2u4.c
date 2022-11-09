/*
 * AdvSIMD vector PCS variant of __v_coshf.
 *
 * Copyright (c) 2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_coshf, _ZGVnN4v_coshf)
#include "v_coshf_2u4.c"
#endif
