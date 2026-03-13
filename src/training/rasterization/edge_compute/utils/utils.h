/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <cstdint>
#include <musa_runtime.h>
#include <iostream>
#include <stdexcept>

#define CHECK_CUDA(debug, name)                                                                                                       \
    if constexpr (debug) {                                                                                                            \
        auto ret = musaDeviceSynchronize();                                                                                           \
        if (ret != musaSuccess) {                                                                                                     \
            std::cerr << "\n[CUDA ERROR] in " << name << " " << __FILE__ << "\nLine " << __LINE__ << ": " << musaGetErrorString(ret); \
            throw std::runtime_error(musaGetErrorString(ret));                                                                        \
        }                                                                                                                             \
    }

template <typename T>
inline __host__ __device__ T div_round_up(T value, T divisor) {
    return (value + divisor - 1) / divisor;
}
