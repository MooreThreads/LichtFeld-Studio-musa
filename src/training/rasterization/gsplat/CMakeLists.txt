cmake_minimum_required(VERSION 3.30)

# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Ensure CUDA targets are available from parent
if(NOT TARGET MUSA::musart)
 message(FATAL_ERROR "MUSA::musart not found - ensure parent CMakeLists.txt sets up CUDA properly")
endif()

if(NOT TARGET MUSA::murand)
 message(FATAL_ERROR "MUSA::murand not found - ensure parent CMakeLists.txt sets up CUDA properly")
endif()

# All gsplat sources together
set(GSPLAT_SOURCES
 # C++ files
 Intersect.cpp
 Projection.cpp
 QuatToRotmat.cpp
 Rasterization.cpp
 Relocation.cpp
 SphericalHarmonics.cpp

 # CUDA files
 IntersectTile.mu
 ProjectionUT3DGSFused.mu
 QuatToRotmatCUDA.mu
 # RasterizeToPixelsFromWorld3DGSBwd.mu  # Excluded: triggers mcc regalloc crash (B2048/B4096)
 RasterizeToPixelsFromWorld3DGSFwd.mu
 RelocationCUDA.mu
 SphericalHarmonicsCUDA.mu
)

# Tell CMake to treat .mu files as CXX (compiled by mcc with -x musa)
set_source_files_properties(
    IntersectTile.mu
    ProjectionUT3DGSFused.mu
    QuatToRotmatCUDA.mu
    RasterizeToPixelsFromWorld3DGSFwd.mu
    RelocationCUDA.mu
    SphericalHarmonicsCUDA.mu
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")

# Note: RasterizeToPixelsFromWorld3DGSBwd.mu excluded from build
# due to mcc regalloc crash (Fatal regalloc, no free temp for B2048/B4096)


# lfs suffix avoids conflict with legacy gsplat_backend
add_library(gsplat_backend_lfs STATIC ${GSPLAT_SOURCES})

set_target_properties(gsplat_backend_lfs PROPERTIES
 CXX_STANDARD 23
 CXX_STANDARD_REQUIRED ON


 POSITION_INDEPENDENT_CODE ON
)

target_include_directories(gsplat_backend_lfs
 PUBLIC
 ${CMAKE_SOURCE_DIR}/include
 ${MUSAToolkit_INCLUDE_DIRS}
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gsplat_backend_lfs
 PUBLIC
 MUSA::musart
 MUSA::murand
 glm::glm
)

target_compile_options(gsplat_backend_lfs PRIVATE
 # C++ non-MSVC (GCC/Clang/mcc)
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0 -g>
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3 -DNDEBUG>
 # C++ MSVC
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od /Z7>
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /DNDEBUG>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
 target_compile_definitions(gsplat_backend_lfs PRIVATE DEBUG DEBUG_BUILD)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
 target_compile_definitions(gsplat_backend_lfs PRIVATE RELEASE_BUILD)
endif()

# GLM CUDA support: GLM_FORCE_CUDA triggers __host__ __device__ annotations.
# Do NOT set GLM_COMPILER=0 as it disables all device annotations.
target_compile_definitions(gsplat_backend_lfs PRIVATE
 GLM_FORCE_CUDA
)

# Print debug info
message(STATUS "gsplat_backend_lfs configuration:")
message(STATUS " - Sources: ${GSPLAT_SOURCES}")
message(STATUS " - Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS " - C++ Standard: 23")
message(STATUS " - CUDA Standard: 20")
message(STATUS " - CUDA include dirs: ${MUSAToolkit_INCLUDE_DIRS}")
