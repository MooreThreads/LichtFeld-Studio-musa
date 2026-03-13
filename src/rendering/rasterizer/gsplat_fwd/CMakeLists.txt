cmake_minimum_required(VERSION 3.30)

# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Forward-only gsplat for viewer (no backward pass, no training dependencies)

# Ensure CUDA targets are available from parent
if(NOT TARGET MUSA::musart)
 message(FATAL_ERROR "MUSA::musart not found - ensure parent CMakeLists.txt sets up CUDA properly")
endif()

# All forward-only gsplat sources
set(GSPLAT_FWD_SOURCES
 # C++ files
 Intersect.cpp
 Projection.cpp
 Rasterization.cpp
 SphericalHarmonics.cpp

 # CUDA files
 IntersectTile.mu
 ProjectionUT3DGSFused.mu
 RasterizeToPixelsFromWorld3DGSFwd.mu
 SphericalHarmonicsCUDA.mu
)

# Tell CMake to treat .mu files as CXX (compiled by mcc with -x musa)
set_source_files_properties(
    IntersectTile.mu
    ProjectionUT3DGSFused.mu
    RasterizeToPixelsFromWorld3DGSFwd.mu
    SphericalHarmonicsCUDA.mu
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")


# Forward-only gsplat backend for rendering
add_library(gsplat_fwd STATIC ${GSPLAT_FWD_SOURCES})

set_target_properties(gsplat_fwd PROPERTIES
 CXX_STANDARD 23
 CXX_STANDARD_REQUIRED ON


 POSITION_INDEPENDENT_CODE ON
)

target_include_directories(gsplat_fwd
 PUBLIC
 ${CMAKE_SOURCE_DIR}/include
 ${MUSAToolkit_INCLUDE_DIRS}
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${CMAKE_CURRENT_SOURCE_DIR}/../utils
)

target_link_libraries(gsplat_fwd
 PUBLIC
 MUSA::musart
 glm::glm
)

target_compile_options(gsplat_fwd PRIVATE
 # CUDA device code + MSVC host compiler flags (Windows only)
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:-O0 -g >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:-O3 >

 # CUDA device code for non-Windows
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0 -g >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3>

 # C++ MSVC
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od /Z7>
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /DNDEBUG>

 # C++ no-MSVC (GCC/Clang/AppleClang/IntelLLVM)
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0 -g>
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3 -DNDEBUG>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
 target_compile_definitions(gsplat_fwd PRIVATE DEBUG DEBUG_BUILD)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
 target_compile_definitions(gsplat_fwd PRIVATE RELEASE_BUILD)
endif()

# GLM CUDA support: GLM_FORCE_CUDA triggers __host__ __device__ annotations.
# Do NOT set GLM_COMPILER=0 as it disables all device annotations.
target_compile_definitions(gsplat_fwd PRIVATE
 GLM_FORCE_CUDA
)

message(STATUS "gsplat_fwd configuration:")
message(STATUS " - Sources: ${GSPLAT_FWD_SOURCES}")
message(STATUS " - Build type: ${CMAKE_BUILD_TYPE}")
