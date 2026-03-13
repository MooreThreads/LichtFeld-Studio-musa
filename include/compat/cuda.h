/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Compatibility shim: provides CUDA driver API types via MUSA equivalents.
 * This allows third-party headers (e.g., ffmpeg hwcontext_cuda.h) that
 * reference <cuda.h> to compile against MUSA.
 *
 * hwcontext_cuda.h does:
 *   #ifndef CUDA_VERSION
 *   #include <cuda.h>
 *   #endif
 * So we define CUDA_VERSION before it can try to include the real cuda.h,
 * and provide the needed typedefs ourselves.
 */

#ifndef LFS_CUDA_H_COMPAT_H_
#define LFS_CUDA_H_COMPAT_H_

// Prevent hwcontext_cuda.h from looking for the real cuda.h
#ifndef CUDA_VERSION
#define CUDA_VERSION 11020
#endif

#include <musa.h>

// Map CUDA driver API types to MUSA equivalents
typedef MUcontext   CUcontext;
typedef MUdeviceptr CUdeviceptr;

// CUstream: hwcontext_cuda.h expects this as an opaque pointer type.
// MUstream is already typedef'd as 'struct MUstream_st *' in musa.h.
typedef MUstream CUstream;

#endif // LFS_CUDA_H_COMPAT_H_
