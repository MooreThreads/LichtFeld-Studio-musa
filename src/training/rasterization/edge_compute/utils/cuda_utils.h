/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <musa_runtime.h>
#include <stdexcept>
#include <string>

#define CHECK_CUDA_PTR(ptr, name)                                                         \
    if (!ptr) {                                                                           \
        throw std::runtime_error("Null pointer for " + std::string(name));                \
    }                                                                                     \
    {                                                                                     \
        musaPointerAttributes attrs;                                                      \
        musaError_t err = musaPointerGetAttributes(&attrs, ptr);                          \
        if (err != musaSuccess) {                                                         \
            musaGetLastError(); /* Clear error */                                         \
            throw std::runtime_error(std::string(name) + " is not a valid CUDA pointer"); \
        }                                                                                 \
        if (attrs.type != musaMemoryTypeDevice) {                                         \
            throw std::runtime_error(std::string(name) + " is not a device pointer");     \
        }                                                                                 \
    }

#define CHECK_CUDA_PTR_OPTIONAL(ptr, name) \
    if (ptr) {                             \
        CHECK_CUDA_PTR(ptr, name)          \
    }

// Simple validation for CUDA pointers without throwing
inline bool is_valid_cuda_ptr(const void* ptr) {
    if (!ptr)
        return false;

    musaPointerAttributes attrs;
    musaError_t err = musaPointerGetAttributes(&attrs, ptr);
    if (err != musaSuccess) {
        musaGetLastError(); // Clear error
        return false;
    }

    return attrs.type == musaMemoryTypeDevice;
}

// Validate that a pointer is accessible from device (more lenient check)
inline bool is_device_accessible(const void* ptr) {
    if (!ptr)
        return false;

    // Try a simple test read
    float test_value;
    musaError_t err = musaMemcpy(&test_value, ptr, sizeof(float), musaMemcpyDeviceToHost);
    if (err != musaSuccess) {
        musaGetLastError(); // Clear error
        return false;
    }

    return true;
}
