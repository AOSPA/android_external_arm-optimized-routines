/*
 * AdvSIMD vector PCS variant of __v_log10f.
 *
 * Copyright (c) 2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_log10f, _ZGVnN4v_log10f)
#include "v_log10f_3u5.c"
#endif
