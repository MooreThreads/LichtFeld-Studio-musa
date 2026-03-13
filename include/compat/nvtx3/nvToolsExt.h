/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * No-op stub for NVTX (NVIDIA Tools Extension Library).
 * MUSA does not have an NVTX equivalent. All annotations are no-ops.
 */

#ifndef LFS_NVTX_COMPAT_H_
#define LFS_NVTX_COMPAT_H_

#include <cstdint>

// No-op range push/pop
inline int nvtxRangePush(const char*) { return 0; }
inline int nvtxRangePushA(const char*) { return 0; }
inline int nvtxRangePop() { return 0; }

// No-op marks
inline void nvtxMark(const char*) {}
inline void nvtxMarkA(const char*) {}

// Range ID type
typedef uint64_t nvtxRangeId_t;
inline nvtxRangeId_t nvtxRangeStartA(const char*) { return 0; }
inline void nvtxRangeEnd(nvtxRangeId_t) {}

// Thread naming
inline void nvtxNameOsThread(uint32_t, const char*) {}

#endif // LFS_NVTX_COMPAT_H_
