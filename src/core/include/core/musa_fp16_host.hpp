/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

// Host-compatible __half type and conversion functions for MUSA
//
// When compiling .cpp files with mcc (not in MUSA device mode), the full
// musa_fp16.h does not expose struct __half because it's guarded behind
// __MUSA_CLANG_ONLY__ (which requires -x musa). This header provides a
// minimal host-side __half so that template instantiations in host code
// (e.g. tensor_expr_impl.hpp) can reference the type and use
// half<->float conversion functions.

#pragma once

#ifdef __MUSACC__
// Device compilation: use the full MUSA fp16 header
#include <musa_fp16.h>
#else
// Host compilation: provide a minimal __half compatible with the device version
#ifndef __HALF_HOST_DEFINED__
#define __HALF_HOST_DEFINED__

#include <cstring>
#include <cstdint>

// Forward-declare conversion functions so __half can use them
struct __half;
inline float __half2float(__half h);
inline __half __float2half(float f);

struct __half {
    unsigned short __x;

    // Allow implicit conversion from float/int/int64_t
    __half() = default;
    __half(float f) { *this = __float2half(f); }
    __half(int i) { *this = __float2half(static_cast<float>(i)); }
    __half(int64_t i) { *this = __float2half(static_cast<float>(i)); }

    // Allow implicit conversion to float
    operator float() const { return __half2float(*this); }
};

struct __half2 {
    unsigned short x;
    unsigned short y;
};

// Host-side half<->float conversion using IEEE 754 bit manipulation
inline float __half2float(__half h) {
    uint16_t bits = h.__x;
    uint32_t sign = (bits & 0x8000u) << 16;
    uint32_t exponent = (bits >> 10) & 0x1Fu;
    uint32_t mantissa = bits & 0x03FFu;

    uint32_t result;
    if (exponent == 0) {
        if (mantissa == 0) {
            result = sign; // +/- zero
        } else {
            // Denormalized: convert to normalized float
            exponent = 1;
            while (!(mantissa & 0x0400u)) {
                mantissa <<= 1;
                exponent--;
            }
            mantissa &= 0x03FFu;
            result = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        result = sign | 0x7F800000u | (mantissa << 13); // inf/nan
    } else {
        result = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }

    float f;
    std::memcpy(&f, &result, sizeof(f));
    return f;
}

inline __half __float2half(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));

    uint32_t sign = (bits >> 16) & 0x8000u;
    int32_t exponent = ((bits >> 23) & 0xFFu) - 127 + 15;
    uint32_t mantissa = bits & 0x007FFFFFu;

    __half h;
    if (exponent <= 0) {
        if (exponent < -10) {
            h.__x = static_cast<unsigned short>(sign); // too small -> zero
        } else {
            mantissa |= 0x00800000u;
            int shift = 1 - exponent + 13;
            h.__x = static_cast<unsigned short>(sign | (mantissa >> shift));
        }
    } else if (exponent >= 31) {
        h.__x = static_cast<unsigned short>(sign | 0x7C00u); // overflow -> inf
    } else {
        h.__x = static_cast<unsigned short>(sign | (exponent << 10) | (mantissa >> 13));
    }
    return h;
}

#endif // __HALF_HOST_DEFINED__
#endif // __MUSACC__
