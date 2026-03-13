/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

// Compatibility shim for std::format
//
// GCC 11 / Clang 14 (mcc) do not provide <format>. This header provides
// std::format, std::format_string, and std::vformat via the fmt library
// which is already a project dependency (through spdlog).

#pragma once

#if __has_include(<format>) && defined(__cpp_lib_format)
// Compiler natively supports <format>
#include <format>
#else
// Fall back to {fmt} library
#include <fmt/format.h>
#include <fmt/core.h>

namespace std {
    using fmt::format;
    using fmt::format_string;
    using fmt::vformat;
    using fmt::make_format_args;
}
#endif
