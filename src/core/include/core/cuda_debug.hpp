/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include "core/logger.hpp"
#include <musa_runtime.h>
#include "core/source_location_compat.hpp"
#include <string>

namespace lfs::core::debug {

    inline void check_cuda_error(const musaError_t err, const char* file, const int line, const char* expr,
                                 const std::source_location loc = std::source_location::current()) {
        if (err != musaSuccess) {
            std::string msg = std::string("CUDA error at ") + file + ":" + std::to_string(line) +
                              " - " + musaGetErrorName(err) + ": " + musaGetErrorString(err) +
                              " (" + expr + ")";
            ::lfs::core::Logger::get().log_internal(
                ::lfs::core::LogLevel::Error, loc, msg);
        }
    }

    inline void check_kernel_sync(const char* file, const int line, const char* kernel_name,
                                  const std::source_location loc = std::source_location::current()) {
        musaError_t err = musaGetLastError();
        if (err != musaSuccess) {
            std::string msg = std::string("Kernel '") + kernel_name + "' launch failed at " +
                              file + ":" + std::to_string(line) +
                              " - " + musaGetErrorName(err) + ": " + musaGetErrorString(err);
            ::lfs::core::Logger::get().log_internal(
                ::lfs::core::LogLevel::Error, loc, msg);
            return;
        }
        err = musaDeviceSynchronize();
        if (err != musaSuccess) {
            std::string msg = std::string("Kernel '") + kernel_name + "' execution failed at " +
                              file + ":" + std::to_string(line) +
                              " - " + musaGetErrorName(err) + ": " + musaGetErrorString(err);
            ::lfs::core::Logger::get().log_internal(
                ::lfs::core::LogLevel::Error, loc, msg);
        }
    }

    inline void check_kernel_async(const char* file, const int line, const char* kernel_name,
                                   const std::source_location loc = std::source_location::current()) {
        const musaError_t err = musaGetLastError();
        if (err != musaSuccess) {
            std::string msg = std::string("Kernel '") + kernel_name + "' launch failed at " +
                              file + ":" + std::to_string(line) +
                              " - " + musaGetErrorName(err) + ": " + musaGetErrorString(err);
            ::lfs::core::Logger::get().log_internal(
                ::lfs::core::LogLevel::Error, loc, msg);
        }
    }

} // namespace lfs::core::debug

#define CHECK_CUDA(call) \
    lfs::core::debug::check_cuda_error((call), __FILE__, __LINE__, #call)

#define CHECK_CUDA_RETURN(call)                                                  \
    do {                                                                         \
        const musaError_t _err = (call);                                         \
        if (_err != musaSuccess) {                                               \
            lfs::core::debug::check_cuda_error(_err, __FILE__, __LINE__, #call); \
            return;                                                              \
        }                                                                        \
    } while (0)

#define CHECK_CUDA_RETURN_VAL(call, val)                                         \
    do {                                                                         \
        const musaError_t _err = (call);                                         \
        if (_err != musaSuccess) {                                               \
            lfs::core::debug::check_cuda_error(_err, __FILE__, __LINE__, #call); \
            return (val);                                                        \
        }                                                                        \
    } while (0)

#ifdef CUDA_DEBUG_SYNC
#define CUDA_KERNEL_CHECK(name) \
    lfs::core::debug::check_kernel_sync(__FILE__, __LINE__, name)

#define CUDA_KERNEL_LAUNCH(kernel, grid, block, shared, stream, ...) \
    do {                                                             \
        kernel<<<grid, block, shared, stream>>>(__VA_ARGS__);        \
        CUDA_KERNEL_CHECK(#kernel);                                  \
    } while (0)
#elif defined(DEBUG_BUILD)
#define CUDA_KERNEL_CHECK(name) \
    lfs::core::debug::check_kernel_async(__FILE__, __LINE__, name)

#define CUDA_KERNEL_LAUNCH(kernel, grid, block, shared, stream, ...) \
    do {                                                             \
        kernel<<<grid, block, shared, stream>>>(__VA_ARGS__);        \
        CUDA_KERNEL_CHECK(#kernel);                                  \
    } while (0)
#else
#define CUDA_KERNEL_CHECK(name) ((void)0)

#define CUDA_KERNEL_LAUNCH(kernel, grid, block, shared, stream, ...) \
    kernel<<<grid, block, shared, stream>>>(__VA_ARGS__)
#endif

#define CUDA_KERNEL(kernel, grid, block, ...) \
    CUDA_KERNEL_LAUNCH(kernel, grid, block, 0, 0, __VA_ARGS__)
