/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

// Compatibility shim for std::source_location
// GCC 11 / Clang 14 (mcc) provide source_location only in <experimental/source_location>.
// This header makes it available as std::source_location regardless of toolchain.

#pragma once

#if __has_include(<source_location>) && defined(__cpp_lib_source_location)
#include <source_location>
#else
#include <experimental/source_location>
namespace std {
    using source_location = experimental::source_location;
}
#endif
