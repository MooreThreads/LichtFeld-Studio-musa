# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
# SPDX-License-Identifier: GPL-3.0-or-later

# ============================================================================
# lfs_core_musa - Unified MUSA library for core module
# Contains: Memory Arena, Lanczos Resize, Core Kernels (kmeans, morton)
# This library is PRIVATE to the core module
# ============================================================================

set(LFS_CORE_MUSA_SOURCES
    memory_arena.mu
    lanczos_resize/lanczos_resize.mu
    undistort/undistort.mu
    kernels/kmeans_new.mu
    kernels/kdtree_kmeans.mu
    kernels/morton_encoding_new.mu
    kernels/selection_ops.mu
    tensor_debug.mu
)

# Tell CMake to treat .mu files as CXX (compiled by mcc with -x musa)
set_source_files_properties(${LFS_CORE_MUSA_SOURCES}
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")

add_library(lfs_core_musa STATIC ${LFS_CORE_MUSA_SOURCES})

target_include_directories(lfs_core_musa
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core/include
        ${CMAKE_BINARY_DIR}/include
        ${MUSAToolkit_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/core/tensor
        ${CMAKE_SOURCE_DIR}/src/core/tensor/internal
)

find_package(OpenMP REQUIRED COMPONENTS CXX)
target_link_libraries(lfs_core_musa
    PUBLIC
        MUSA::musart
        MUSA::musa_driver
        MUSA::murand
        spdlog::spdlog
        glm::glm
        OpenMP::OpenMP_CXX
)

set_target_properties(lfs_core_musa PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# MUSA compiler options for .mu files
target_compile_options(lfs_core_musa PRIVATE -O2)

message(STATUS "lfs_core_musa library configured (memory arena, lanczos resize, core kernels)")
