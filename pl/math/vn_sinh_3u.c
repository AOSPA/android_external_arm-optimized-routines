/*
 * AdvSIMD vector PCS variant of __v_sinh.
 *
 * Copyright (c) 2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "include/mathlib.h"
#ifdef __vpcs
#define VPCS 1
#define VPCS_ALIAS strong_alias (__vn_sinh, _ZGVnN2v_sinh)
#include "v_sinh_3u.c"
#endif
