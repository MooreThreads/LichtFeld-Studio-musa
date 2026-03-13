/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <cstdint>
#include <musa_runtime.h>

namespace gsplat_lfs {

    void launch_quats_to_rotmats_kernel(
        const float* quats, // [N, 4]
        int64_t N,
        float* rotmats, // [N, 3, 3]
        musaStream_t stream = nullptr);

} // namespace gsplat_lfs
