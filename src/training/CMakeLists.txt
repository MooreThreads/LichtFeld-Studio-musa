# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Add fastlfs backend
add_subdirectory(rasterization/fastgs)

# Add LibTorch-free gsplat backend
add_subdirectory(rasterization/gsplat)

# Add igs+ edge-compute rasterizer
add_subdirectory(rasterization/edge_compute)

# LibTorch-free CUDA kernels
set(LFS_TRAINING_KERNEL_SOURCES
 kernels/ssim.mu
 kernels/l1_loss.mu
 kernels/ssim_reduction.mu
 kernels/regularization.mu
 kernels/bilateral_grid_forward.mu
 kernels/bilateral_grid_backward.mu
 kernels/bilateral_grid_tv.mu
 kernels/ppisp.mu
 kernels/mcmc_kernels.mu
 kernels/densification_kernels.mu
 kernels/grad_alpha.mu
 kernels/image_kernels.mu
 components/sparsity_optimizer_kernels.mu
)

# Tell CMake to treat .mu files as CXX (compiled by mcc with -x musa)
set_source_files_properties(${LFS_TRAINING_KERNEL_SOURCES}
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")


# Create CUDA kernels library
add_library(lfs_training_kernels STATIC ${LFS_TRAINING_KERNEL_SOURCES})

target_include_directories(lfs_training_kernels
 PUBLIC
 $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
 $<INSTALL_INTERFACE:include>
)

target_link_libraries(lfs_training_kernels
 PUBLIC
 lfs_core # For lfs::core::Tensor (tensor merged into lfs_core)
 MUSA::musart
)

set_target_properties(lfs_training_kernels PROPERTIES

 POSITION_INDEPENDENT_CODE ON

)

# CUDA compiler options for lfs_training_kernels
target_compile_options(lfs_training_kernels PRIVATE
 # CUDA device code + MSVC host compiler flags (Windows only)
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:-O0 -g -8 >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:-O3 -8 >

 # CUDA device code for non-Windows
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0 -g >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3 >
)

if(MSVC)
 target_compile_definitions(lfs_training_kernels PRIVATE
 $<$<COMPILE_LANGUAGE:CXX>:FMT_UNICODE=0>
 )
endif()
# Optimizer sources
set(TRAINING_NEW_SOURCES
 optimizer/adam_optimizer.cpp
 optimizer/scheduler.cpp
 losses/regularization.cpp
 losses/photometric_loss.cpp
 strategies/strategy_utils.cpp
 strategies/adc.cpp
 strategies/mcmc.cpp
 strategies/strategy_factory.cpp
 strategies/improved_gs_plus.cpp
 rasterization/fast_rasterizer.cpp
 rasterization/gsplat_rasterizer.cpp
 rasterization/edge_rasterizer.cpp
 components/bilateral_grid.cpp
 components/ppisp.cpp
 components/ppisp_controller.mu
 components/ppisp_controller_pool.mu
 components/ppisp_file.cpp
 components/sparsity_optimizer.cpp
 control/command_api.cpp
 metrics/metrics.cpp
 checkpoint.cpp
 trainer.cpp
 training_setup.cpp
)

# Tell CMake to treat .mu files as CXX (compiled by mcc with -x musa)
set_source_files_properties(
    components/ppisp_controller.mu
    components/ppisp_controller_pool.mu
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")


# lfs_training - Training module with optimizer
add_library(lfs_training STATIC ${TRAINING_NEW_SOURCES})

target_include_directories(lfs_training
 PUBLIC
 $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
 $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
 $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/external> # For indicators.hpp (used in progress.hpp)
 $<INSTALL_INTERFACE:include>
 PRIVATE
 ${CMAKE_SOURCE_DIR}/src # For old training components (TODO: port)
 ${CMAKE_SOURCE_DIR}/src/training # For old training components (TODO: port)
 ${CMAKE_SOURCE_DIR}/src/training/fastgs # For adam_api.h (old)
 ${CMAKE_CURRENT_SOURCE_DIR}/rasterization/fastgs/rasterization/include # For rasterization_api.h
 ${CMAKE_CURRENT_SOURCE_DIR}/rasterization/fastgs/optimizer/include
 ${CMAKE_CURRENT_SOURCE_DIR}/rasterization/fastgs/utils
	${CMAKE_CURRENT_SOURCE_DIR}/rasterization/edge_compute/rasterization/include
 ${CMAKE_CURRENT_SOURCE_DIR}/rasterization/edge_compute/utils
 ${CMAKE_CURRENT_SOURCE_DIR}/rasterization # For gsplat headers
)

# Link dependencies
target_link_libraries(lfs_training
 PUBLIC
 lfs_core # Depends on new core module (SplatData, Tensor)
 lfs_io # For CacheLoader, exporter, pipelined loader
 lfs_event_bridge # Shared event bridge for cross-module singletons
 lfs_training_kernels # LibTorch-free SSIM kernels
 fastlfs_backend # For adam_step_raw CUDA kernel (LibTorch-free)
 gsplat_backend_lfs # LibTorch-free gsplat rasterization backend
	edge_compute_backend # For igs+ edge score rasterization
 PRIVATE
 lfs_python_utils
)

# Set C++ standard
set_target_properties(lfs_training PROPERTIES
 CXX_STANDARD 23
 CXX_STANDARD_REQUIRED ON
 POSITION_INDEPENDENT_CODE ON
)

message(STATUS "╔════════════════════════════════════════════════════════════════╗")
message(STATUS "║ lfs_training - Training Module with Optimizer & Losses ║")
message(STATUS "╚════════════════════════════════════════════════════════════════╝")
message(STATUS " • Contains: AdamOptimizer, LR Schedulers, Losses (LibTorch-free API)")
message(STATUS " • Namespace: lfs::training")
message(STATUS " • Uses: CUDA kernels from gs_training_kernels, fastgs_backend")
