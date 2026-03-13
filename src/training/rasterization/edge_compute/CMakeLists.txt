# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.30)

set(EDGE_RASTERIZER_SOURCES
 # rasterizer
 ${CMAKE_CURRENT_SOURCE_DIR}/rasterization/src/rasterization_api.mu
 ${CMAKE_CURRENT_SOURCE_DIR}/rasterization/src/forward.mu
)

# Tell CMake to treat .mu files as CXX (compiled by mcc with -x musa)
set_source_files_properties(${EDGE_RASTERIZER_SOURCES}
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")


# One unified library - renamed from fastgs_backend to fastlfs_backend
add_library(edge_compute_backend STATIC ${EDGE_RASTERIZER_SOURCES})

set_target_properties(edge_compute_backend PROPERTIES


 POSITION_INDEPENDENT_CODE ON
)

target_include_directories(edge_compute_backend
 PUBLIC
 ${CMAKE_CURRENT_SOURCE_DIR}/rasterization/include
	${CMAKE_CURRENT_SOURCE_DIR}/rasterization/src
 ${CMAKE_CURRENT_SOURCE_DIR}/utils
 ${CMAKE_SOURCE_DIR}/include
 ${MUSAToolkit_INCLUDE_DIRS}
 PRIVATE
)

# Pure CUDA libraries only
target_link_libraries(edge_compute_backend
 PUBLIC
 MUSA::musart
 MUSA::murand
 MUSA::musa_driver
 lfs_core_musa # MUSA utilities from core module (memory arena)
)

target_compile_options(edge_compute_backend PRIVATE
 # CUDA device code + MSVC host compiler flags (Windows only)
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:-O0 -g >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:-O3 >

 # CUDA on Windows: Pass /utf-8 to MSVC when invoked by nvcc
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:
 -8
 
 # Suppress "character value out of range" warnings from fmt
 >

 # C++ MSVC
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od /Z7>
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /DNDEBUG>

 # C++ no-MSVC (GCC/Clang/AppleClang/IntelLLVM)
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0 -g>
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3 -DNDEBUG>
)

# Windows: Disable fmt Unicode check for CUDA files (nvcc doesn't properly propagate /utf-8)
if(MSVC)
 target_compile_definitions(edge_compute_backend PRIVATE
 $<$<COMPILE_LANGUAGE:CXX>:FMT_UNICODE=0>
 )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
 target_compile_definitions(edge_compute_backend PRIVATE _DEBUG DEBUG_BUILD)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
 target_compile_definitions(edge_compute_backend PRIVATE RELEASE_BUILD)
endif()
