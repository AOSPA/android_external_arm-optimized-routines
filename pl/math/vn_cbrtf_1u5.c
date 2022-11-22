/*
 * AdvSIMD vector PCS variant of __v_cbrtf.
 *
 * Copyright (c) 2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_cbrtf, _ZGVnN4v_cbrtf)
#include "v_cbrtf_1u5.c"
#endif
