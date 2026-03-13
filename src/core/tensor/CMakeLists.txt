# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
# SPDX-License-Identifier: GPL-3.0-or-later

# ============================================================================
# lfs_tensor_kernels - CUDA kernels for tensor operations
# The C++ tensor sources are now compiled as part of lfs_core (SHARED).
# Only the CUDA .cu files remain here as a STATIC library linked privately.
# ============================================================================

set(LFS_TENSOR_CUDA_SOURCES
 tensor_ops.mu
 tensor_warp_reduce.mu
 tensor_matrix_ops.mu
 tensor_broadcast_ops.mu
 tensor_masking_ops.mu
 tensor_random_ops.mu
 tensor_strided_ops.mu
 tensor_dot_optimized.mu
 tensor_nn_ops.mu
 tensor_fused_pointwise.mu
)

# Tell CMake to treat .mu files as CXX (compiled by mcc with -x musa)
set_source_files_properties(${LFS_TENSOR_CUDA_SOURCES}
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")


if(LFS_TENSOR_CUDA_SOURCES)
 add_library(lfs_tensor_kernels STATIC ${LFS_TENSOR_CUDA_SOURCES})

 set_target_properties(lfs_tensor_kernels PROPERTIES


 POSITION_INDEPENDENT_CODE ON

 )

 target_include_directories(lfs_tensor_kernels
 PUBLIC
 ${CMAKE_CURRENT_SOURCE_DIR}/../include
 ${CMAKE_BINARY_DIR}/include
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${MUSAToolkit_INCLUDE_DIRS}
 )

 target_link_libraries(lfs_tensor_kernels
 PUBLIC
 MUSA::musart
 MUSA::murand
 spdlog::spdlog
 )

 target_compile_options(lfs_tensor_kernels PRIVATE
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:-O0 -g -8 >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:-O3 -8 >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0 -g >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3 >
 )

 target_compile_definitions(lfs_tensor_kernels PRIVATE LFS_CORE_EXPORTS)

 if(MSVC)
 target_compile_definitions(lfs_tensor_kernels PRIVATE
 $<$<COMPILE_LANGUAGE:CXX>:FMT_UNICODE=0>
 )
 endif()

 # Windows NVCC 12.8 ICE workaround
 if(WIN32)
 set_source_files_properties(
 tensor_broadcast_ops.mu
 tensor_ops.mu
 PROPERTIES
 COMPILE_FLAGS "-O0"
 )
 endif()
endif()
