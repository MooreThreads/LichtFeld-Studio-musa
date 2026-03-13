# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
#
# SPDX-License-Identifier: GPL-3.0-or-later

# windows/linux platform independent
# vcpkg support (optional - skip if VCPKG_ROOT not set)
set(RAW_VCPKG_PATH $ENV{VCPKG_ROOT})
if(RAW_VCPKG_PATH)
 file(TO_CMAKE_PATH "${RAW_VCPKG_PATH}" VCPKG_ROOT)
 if ("${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
  message(STATUS "Using vcpkg toolchain: ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
 endif()
else()
 message(STATUS "VCPKG_ROOT not set, skipping vcpkg toolchain")
endif()

cmake_minimum_required(VERSION 3.30)

# Suppress CMake developer warnings
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release build
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
 message(STATUS "No build type specified, defaulting to Release")
 set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
 set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

project(LichtFeld-Studio LANGUAGES CXX C)

# Disable C++20 module scanning
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Get git version info
execute_process(
 COMMAND git rev-parse --short HEAD
 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 OUTPUT_VARIABLE GIT_COMMIT_HASH_SHORT
 OUTPUT_STRIP_TRAILING_WHITESPACE
 ERROR_QUIET
)
execute_process(
 COMMAND git describe --dirty
 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 OUTPUT_VARIABLE GIT_TAGGED_VERSION
 OUTPUT_STRIP_TRAILING_WHITESPACE
 ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH_SHORT)
 set(GIT_COMMIT_HASH_SHORT "unknown")
endif()
if(NOT GIT_TAGGED_VERSION)
 set(GIT_TAGGED_VERSION "unknown")
endif()

# Generate git_version.h at configure time
file(WRITE "${CMAKE_BINARY_DIR}/include/git_version.h"
"#pragma once\n\
#define GIT_COMMIT_HASH_SHORT \"${GIT_COMMIT_HASH_SHORT}\"\n\
#define GIT_TAGGED_VERSION \"${GIT_TAGGED_VERSION}\"\n"
)

# Setup
get_filename_component(PROJ_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

option(ENABLE_CUDA_GL_INTEROP "Enable CUDA-OpenGL interoperability" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_UNICODE_TEST_ONLY "Build only Unicode path test (no CUDA)" OFF)
option(ENABLE_ALLOCATION_PROFILING "Enable tensor allocation profiling" OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Debug options
option(ENABLE_TENSOR_VALIDATION "Enable runtime tensor NaN/Inf validation" OFF)
option(ENABLE_CUDA_DEBUG_SYNC "Enable synchronous CUDA kernel checking (slow)" OFF)
option(ENABLE_TENSOR_OP_TRACING "Enable tensor operation tracing" OFF)

# Add -rdynamic for backtrace symbol resolution
if(UNIX AND NOT APPLE)
 set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
endif()

# PTX-only: smaller binary (~66MB vs ~750MB), JIT compiles at runtime
option(BUILD_CUDA_PTX_ONLY "Build PTX-only (JIT at runtime, ~5-15s first startup)" OFF)

# Portable: PTX-only + bundled libs, no tests
option(BUILD_PORTABLE "Build portable distribution" OFF)
if(BUILD_PORTABLE)
 set(BUILD_CUDA_PTX_ONLY ON CACHE BOOL "" FORCE)
 set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
 message(STATUS "Portable: PTX-only, tests disabled")
endif()

# Compiler cache (CUDA caching disabled on Windows due to nvcc compatibility issues)
find_program(SCCACHE_PROGRAM sccache)
find_program(CCACHE_PROGRAM ccache)
if(SCCACHE_PROGRAM)
 set(CMAKE_C_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
 set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
 if(WIN32)
 message(STATUS "Compiler cache: sccache (C/C++ only, CUDA uncached)")
 else()
 set(CMAKE_MU_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
 message(STATUS "Compiler cache: sccache")
 endif()
elseif(CCACHE_PROGRAM)
 set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
 set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
 if(WIN32)
 message(STATUS "Compiler cache: ccache (C/C++ only, CUDA uncached)")
 else()
 set(CMAKE_MU_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
 message(STATUS "Compiler cache: ccache")
 endif()
endif()

# Minimum SM for PTX-only (75=Turing, 80=Ampere, 86=GA102, 89=Ada, 90=Hopper)
set(BUILD_CUDA_MIN_SM "75" CACHE STRING "Minimum SM for PTX-only builds")

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Parallel build
include(ProcessorCount)
ProcessorCount(total_cores)
if(total_cores GREATER 1)
 math(EXPR used_cores "${total_cores} - 2")
 set(ENV{MAKEFLAGS} "-j${used_cores}")
 message(STATUS "Building with ${used_cores} cores")
endif()

# MUSA setup (Moore Threads GPU)
# Detect MT GPU via mthreads-gmi
find_program(MTHREADS_GMI mthreads-gmi)
if(MTHREADS_GMI)
 message(STATUS "Detected Moore Threads GPU via mthreads-gmi")
else()
 message(WARNING "mthreads-gmi not found, using default MUSA architecture")
endif()

# Set MUSA architecture for MT S-series GPUs
set(LichtFeld-Studio_MUSA_ARCH "mp_22" CACHE STRING "MUSA architecture")
message(STATUS "MUSA architecture: ${LichtFeld-Studio_MUSA_ARCH}")

# MUSA compiler and toolkit
set(CMAKE_MU_COMPILER /usr/local/musa/bin/mcc)
set(MUSA_PATH /usr/local/musa CACHE PATH "MUSA SDK path")

# MUSA include and library paths
set(MUSAToolkit_INCLUDE_DIRS ${MUSA_PATH}/include)
set(MUSAToolkit_LIBRARY_DIR ${MUSA_PATH}/lib)

# Find MUSA libraries
find_library(MUSART_LIBRARY NAMES musart PATHS ${MUSA_PATH}/lib NO_DEFAULT_PATH)
find_library(MUSA_DRIVER_LIBRARY NAMES musa
 PATHS ${MUSA_PATH}/lib ${MUSA_PATH}/lib/stubs /usr/lib/x86_64-linux-gnu)
find_library(MURAND_LIBRARY NAMES murand PATHS ${MUSA_PATH}/lib NO_DEFAULT_PATH)

# Create imported targets
if(NOT TARGET MUSA::musart)
 add_library(MUSA::musart SHARED IMPORTED)
 set_target_properties(MUSA::musart PROPERTIES
 IMPORTED_LOCATION ${MUSART_LIBRARY}
 INTERFACE_INCLUDE_DIRECTORIES ${MUSAToolkit_INCLUDE_DIRS}
 )
endif()

if(NOT TARGET MUSA::musa_driver AND MUSA_DRIVER_LIBRARY)
 add_library(MUSA::musa_driver SHARED IMPORTED)
 set_target_properties(MUSA::musa_driver PROPERTIES
 IMPORTED_LOCATION ${MUSA_DRIVER_LIBRARY}
 )
endif()

if(NOT TARGET MUSA::murand AND MURAND_LIBRARY)
 add_library(MUSA::murand SHARED IMPORTED)
 set_target_properties(MUSA::murand PROPERTIES
 IMPORTED_LOCATION ${MURAND_LIBRARY}
 INTERFACE_INCLUDE_DIRECTORIES ${MUSAToolkit_INCLUDE_DIRS}
 )
endif()

# MU compiler flags

message(STATUS "MUSA SDK found at ${MUSA_PATH}")

# nvToolsExt is not needed for MUSA
message(STATUS "MUSA SDK ${LichtFeld-Studio_MUSA_ARCH} configured")

# C++ standard library compat shims for mcc (Clang 14 / libstdc++ 11)
# Provides <format> via fmt and <expected> polyfill
include_directories(BEFORE SYSTEM ${CMAKE_SOURCE_DIR}/include/compat)

# Find packages
find_package(TBB REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glad REQUIRED)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)
find_package(implot REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(args REQUIRED)
find_package(spdlog REQUIRED)
find_package(Freetype REQUIRED)
find_package(WebP REQUIRED)
find_package(LibArchive REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(assimp REQUIRED)
find_package(RmlUi CONFIG REQUIRED)

# Set MUSA architecture
set(CMAKE_MU_ARCHITECTURES "${LichtFeld-Studio_MUSA_ARCH}" CACHE STRING "MUSA architectures" FORCE)

# MUSA-GL interop - disable for now (may not be fully supported on MT GPUs)
# The project has a CPU fallback path when interop is disabled
if(ENABLE_CUDA_GL_INTEROP)
 # Check if musa_gl_interop.h exists
 if(EXISTS "${MUSA_PATH}/include/musa_gl_interop.h")
 message(STATUS "MUSA-GL interop support: ENABLED (header found)")
 set(CUDA_GL_INTEROP_FOUND TRUE)
 set(CUDA_GL_INTEROP_ENABLED 1)
 else()
 message(WARNING "MUSA-GL interop support: DISABLED (header not found)")
 set(CUDA_GL_INTEROP_FOUND FALSE)
 set(CUDA_GL_INTEROP_ENABLED 0)
 endif()
else()
 message(STATUS "MUSA-GL interop support: DISABLED (by user option)")
 set(CUDA_GL_INTEROP_FOUND FALSE)
 set(CUDA_GL_INTEROP_ENABLED 0)
endif()

# Create configuration header (static options)
configure_file(
 "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
 "${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
 @ONLY
)


# Project-wide definitions
add_definitions(-DPROJECT_ROOT_PATH="${PROJ_ROOT_DIR}")

# CUDA device debugging (-G flag) generates huge object files. Disable in CI.
option(CUDA_DEVICE_DEBUG "Enable CUDA device debugging (-G flag)" ON)

if(CUDA_DEVICE_DEBUG)
 set(LFS_CUDA_DEVICE_DEBUG_FLAGS "-G" CACHE INTERNAL "")
else()
 set(LFS_CUDA_DEVICE_DEBUG_FLAGS "" CACHE INTERNAL "")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
 set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
 if(CUDA_DEVICE_DEBUG)
 set(CMAKE_MU_FLAGS_DEBUG "${CMAKE_MU_FLAGS_DEBUG} -g -G -O0")
 else()
 set(CMAKE_MU_FLAGS_DEBUG "${CMAKE_MU_FLAGS_DEBUG} -g -O0")
 endif()
endif()

# External dependencies (EXCLUDE_FROM_ALL to handle install ourselves)
add_subdirectory(external EXCLUDE_FROM_ALL)

# =============================================================================
# LIBRARY MODULES
# =============================================================================
add_subdirectory(src/core) # Core module (lfs::core namespace)
add_subdirectory(src/training) # Training module (lfs::training namespace)
add_subdirectory(src/io) # I/O module (lfs::io namespace)
add_subdirectory(src/rendering) # Rendering module (lfs::rendering namespace)
add_subdirectory(src/sequencer) # Sequencer module (lfs::sequencer namespace)
add_subdirectory(src/visualizer) # Visualizer module (lfs::vis namespace)
add_subdirectory(src/geometry) # Geometry utilities
add_subdirectory(src/mcp) # MCP server (AI agent integration)
add_subdirectory(src/python)

# =============================================================================
# MAIN EXECUTABLES
# =============================================================================

# -----------------------------------------------------------------------------
# LichtFeld-Studio
# -----------------------------------------------------------------------------

add_executable(${PROJECT_NAME}
 src/app/main.cpp
 src/app/application.cpp
 src/app/converter.cpp
 src/app/splash_screen.cpp
 resources/lichtfeld-studio.rc
)


# No special properties needed for MUSA build

target_include_directories(${PROJECT_NAME}
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}/include
 ${CMAKE_CURRENT_BINARY_DIR}/include
 ${CMAKE_CURRENT_SOURCE_DIR}/src/app/include # For app headers
 ${CMAKE_CURRENT_SOURCE_DIR}/src # For training/, visualizer/, etc.
 ${CMAKE_CURRENT_SOURCE_DIR}/external # For external headers

 ${MUSAToolkit_INCLUDE_DIRS}
 ${OPENGL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
 PRIVATE
 lfs_visualizer
 lfs_mcp
 lfs_training
 lfs_geometry
 lfs_core
 lfs_io
 lfs_video
 fastlfs_backend
 lfs_python_utils

 ${OPENGL_LIBRARIES}
 MUSA::musart
 spdlog::spdlog
 fmt::fmt
 taywee::args
 OpenImageIO::OpenImageIO
 glad::glad
 SDL3::SDL3
)

# Copy nvImageCodec shared libraries next to executable for easy deployment
if(TARGET nvimgcodec)
 # Force build of nvimgcodec even with EXCLUDE_FROM_ALL
 add_dependencies(${PROJECT_NAME} nvimgcodec)

 # Add post-build command to copy libraries and create symlinks (Unix only)
 add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "$<TARGET_FILE:nvimgcodec>"
 "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
 COMMENT "Copying nvImageCodec library"
 )

 # Create symlinks only on Unix-like systems (not Windows)
 if(UNIX)
 add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E create_symlink
 "$<TARGET_FILE_NAME:nvimgcodec>"
 "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libnvimgcodec.so.0"
 COMMAND ${CMAKE_COMMAND} -E create_symlink
 "libnvimgcodec.so.0"
 "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libnvimgcodec.so"
 COMMENT "Creating nvImageCodec symlinks"
 )
 endif()

 message(STATUS "Post-build: nvImageCodec libraries will be copied to executable directory")
endif()

# Copy Python DLLs for Windows runtime (Python is embedded in the application)
if(WIN32)
 get_filename_component(VCPKG_PYTHON_DIR "${Python_EXECUTABLE}" DIRECTORY)

 # Glob python*.dll - check both tools/python3/ and parent bin/ (vcpkg layout varies)
 file(GLOB PYTHON_RUNTIME_DLLS
 "${VCPKG_PYTHON_DIR}/python*.dll"
 "${VCPKG_PYTHON_DIR}/../bin/python*.dll"
 )
 if(PYTHON_RUNTIME_DLLS)
 add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 ${PYTHON_RUNTIME_DLLS}
 "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
 COMMENT "Copying Python DLLs to executable directory"
 )
 message(STATUS "Post-build: Python DLLs will be copied: ${PYTHON_RUNTIME_DLLS}")
 endif()
endif()

# MUSA runtime library copying (Linux only - no Windows support for MUSA)

# Before extension post-build copying: remove stale 'extensions' file if present (prevent mkdir failure)
set(_target_extensions_dir "${CMAKE_BINARY_DIR}/extensions")
if(EXISTS "${_target_extensions_dir}" AND NOT IS_DIRECTORY "${_target_extensions_dir}")
 file(REMOVE "${_target_extensions_dir}")
 message(STATUS "Removed stale file '${_target_extensions_dir}' to make way for extensions/ directory")
endif()

# =============================================================================
# EXTENSIONS COPYING - Copy nvImageCodec extensions to build/extensions
# =============================================================================
if(TARGET nvjpeg_ext)
 # Force build of nvjpeg_ext even with EXCLUDE_FROM_ALL
 add_dependencies(${PROJECT_NAME} nvjpeg_ext)

 add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/extensions"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "$<TARGET_FILE:nvjpeg_ext>"
 "$<TARGET_FILE_DIR:${PROJECT_NAME}>/extensions/"
 COMMENT "Copying nvjpeg extension binary only"
 )
endif()

if(TARGET nvjpeg2k_ext)
 # Force build of nvjpeg2k_ext even with EXCLUDE_FROM_ALL
 add_dependencies(${PROJECT_NAME} nvjpeg2k_ext)

 add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/extensions"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "$<TARGET_FILE:nvjpeg2k_ext>"
 "$<TARGET_FILE_DIR:${PROJECT_NAME}>/extensions/"
 COMMENT "Copying nvjpeg2k extension binary only"
 )
endif()

# =============================================================================
# RESOURCE COPYING - Copy all resources to build/resources for runtime access
# =============================================================================
set(RESOURCE_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources")

# Create resource directory structure
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DIR}/shaders"
 COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DIR}/assets/icon"
 COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DIR}/assets/icon/scene"
 COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DIR}/assets/fonts"
 COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DIR}/assets/themes"
 COMMENT "Creating resource directory structure"
)

# Copy rendering shaders
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
 "${CMAKE_SOURCE_DIR}/src/rendering/resources/shaders"
 "${RESOURCE_DIR}/shaders"
 COMMENT "Copying rendering shaders"
)

# Copy visualizer icons
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/icon"
 "${RESOURCE_DIR}/assets/icon"
 COMMENT "Copying visualizer icons"
)

# Copy fonts (from visualizer)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/fonts"
 "${RESOURCE_DIR}/assets/fonts"
 COMMENT "Copying visualizer fonts"
)

# Copy fonts (from rendering - JetBrainsMono for viewport gizmo)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_SOURCE_DIR}/src/rendering/resources/assets/JetBrainsMono-Regular.ttf"
 "${RESOURCE_DIR}/assets/fonts/"
 COMMENT "Copying rendering fonts"
)

# Copy themes
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/themes"
 "${RESOURCE_DIR}/assets/themes"
 COMMENT "Copying themes"
)

# Copy additional assets (logo, splash images, etc.)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-icon.png"
 "${RESOURCE_DIR}/assets/"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-splash-logo.png"
 "${RESOURCE_DIR}/assets/"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-splash-logo-dark.png"
 "${RESOURCE_DIR}/assets/"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-splash-loading.png"
 "${RESOURCE_DIR}/assets/"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-splash-loading-dark.png"
 "${RESOURCE_DIR}/assets/"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/core11-logo.png"
 "${RESOURCE_DIR}/assets/"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/core11-logo-dark.png"
 "${RESOURCE_DIR}/assets/"
 COMMENT "Copying application assets"
)

# Copy locale files
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
 "${CMAKE_SOURCE_DIR}/src/visualizer/gui/resources/locales"
 "${RESOURCE_DIR}/locales"
 COMMENT "Copying locale files"
)

message(STATUS "Post-build: Resources will be copied to \${BUILD_DIR}/resources/")

# Platform-specific settings
if(UNIX)
 target_link_libraries(${PROJECT_NAME} PRIVATE GL GLU)

 if(BUILD_PORTABLE)
 set_target_properties(${PROJECT_NAME} PROPERTIES
 INSTALL_RPATH "$ORIGIN:$ORIGIN/lib"
 BUILD_WITH_INSTALL_RPATH TRUE
 INSTALL_RPATH_USE_LINK_PATH FALSE
 )
 else()
 set_target_properties(${PROJECT_NAME} PROPERTIES
 INSTALL_RPATH "$ORIGIN:$ORIGIN/lib:${MUSAToolkit_LIBRARY_DIR}"
 BUILD_WITH_INSTALL_RPATH TRUE
 INSTALL_RPATH_USE_LINK_PATH TRUE
 )
 endif()

elseif(WIN32)
 # On Windows, DLLs in the same directory as the .exe are automatically found
 # For multi-config generators (MSVC), set all configs to same directory
 set_target_properties(${PROJECT_NAME} PROPERTIES
 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
 RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
 RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
 RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
 RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
 )
endif()

# Build type configuration function
function(configure_build_type target)
 get_target_property(target_type ${target} TYPE)

 if(target_type STREQUAL "INTERFACE_LIBRARY")
 if(CMAKE_BUILD_TYPE STREQUAL "Debug")
 target_compile_definitions(${target} INTERFACE DEBUG_BUILD)
 elseif(CMAKE_BUILD_TYPE STREQUAL "Release|RelWithDebInfo|MinSizeRel")
 target_compile_definitions(${target} INTERFACE RELEASE_BUILD)
 endif()
 # Debug options for interface libraries
 if(ENABLE_TENSOR_VALIDATION)
 target_compile_definitions(${target} INTERFACE TENSOR_VALIDATION_ENABLED)
 endif()
 if(ENABLE_CUDA_DEBUG_SYNC)
 target_compile_definitions(${target} INTERFACE CUDA_DEBUG_SYNC)
 endif()
 if(ENABLE_TENSOR_OP_TRACING)
 target_compile_definitions(${target} INTERFACE TENSOR_OP_TRACING)
 endif()
 else()
 if(CMAKE_BUILD_TYPE STREQUAL "Debug")
 target_compile_definitions(${target} PRIVATE DEBUG_BUILD)
 elseif(CMAKE_BUILD_TYPE STREQUAL "Release|RelWithDebInfo|MinSizeRel")
 target_compile_definitions(${target} PRIVATE RELEASE_BUILD)
 endif()
 # Debug options for regular libraries
 if(ENABLE_TENSOR_VALIDATION)
 target_compile_definitions(${target} PRIVATE TENSOR_VALIDATION_ENABLED)
 endif()
 if(ENABLE_CUDA_DEBUG_SYNC)
 target_compile_definitions(${target} PRIVATE CUDA_DEBUG_SYNC)
 endif()
 if(ENABLE_TENSOR_OP_TRACING)
 target_compile_definitions(${target} PRIVATE TENSOR_OP_TRACING)
 endif()
 endif()
endfunction()

# Apply build configuration to all targets
configure_build_type(fastlfs_backend)
configure_build_type(lfs_core)
configure_build_type(lfs_core_musa)
configure_build_type(lfs_training)
configure_build_type(lfs_rendering)
configure_build_type(lfs_visualizer)
configure_build_type(lfs_io)
configure_build_type(lfs_geometry)
configure_build_type(${PROJECT_NAME})

# =============================================================================
# TESTING (Optional)
# =============================================================================
if(BUILD_TESTS OR BUILD_UNICODE_TEST_ONLY)
 enable_testing()
 add_subdirectory(tests)
 if(BUILD_UNICODE_TEST_ONLY)
 message(STATUS "Unicode path test enabled. Build with 'ninja unicode_path_test' and run with 'ctest -R UnicodePathTest'")
 else()
 message(STATUS "Tests enabled. Build with 'ninja lichtfeld_tests' and run with 'ninja run_tests' or 'ctest'")
 endif()
endif()

# =============================================================================
# BUILD INFO & OPTIMIZATIONS
# =============================================================================
message(STATUS "===========================================")
message(STATUS "Build Configuration:")
message(STATUS " MUSA Arch: ${LichtFeld-Studio_MUSA_ARCH}")
message(STATUS " MUSA SDK: ${MUSA_PATH}")
message(STATUS " OpenGL Found: ${OPENGL_FOUND}")
message(STATUS " FreeType Found: ${FREETYPE_FOUND}")
message(STATUS " WebP Found: ${WebP_FOUND}")
message(STATUS " LibArchive Found: ${LibArchive_FOUND}")
message(STATUS " MUSA-GL Interop Option: ${ENABLE_CUDA_GL_INTEROP}")
message(STATUS " MUSA-GL Interop Available: ${CUDA_GL_INTEROP_FOUND}")
message(STATUS " Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS " C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS " MU Standard: ${CMAKE_MU_STANDARD}")
message(STATUS " OpenImageIO Found: ${OpenImageIO_FOUND}")
message(STATUS " Tests: ${BUILD_TESTS}")
message(STATUS " Unicode Test Only: ${BUILD_UNICODE_TEST_ONLY}")
message(STATUS " Portable Build: ${BUILD_PORTABLE}")
if(BUILD_PORTABLE)
 message(STATUS " -> PTX-only: ON (SM >= ${BUILD_CUDA_MIN_SM}, JIT at runtime)")
 message(STATUS " -> Use: cmake --install build --prefix ./dist")
endif()
message(STATUS "===========================================")

# =============================================================================
# INSTALLATION (cmake --install build --prefix ./dist)
# =============================================================================
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
 set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()

set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
set(CMAKE_INSTALL_OPENMP_LIBRARIES TRUE)

include(GNUInstallDirs)
include(InstallRequiredSystemLibraries)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS lfs_core
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS lfs_logger
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS lfs_event_bridge
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS lfs_visualizer
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS lfs_mcp
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION ".")

set(INSTALL_RESOURCE_DIR "${CMAKE_INSTALL_DATADIR}/LichtFeld-Studio")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/rendering/resources/shaders" DESTINATION "${INSTALL_RESOURCE_DIR}")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/icon" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/fonts" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(FILES "${CMAKE_SOURCE_DIR}/src/rendering/resources/assets/JetBrainsMono-Regular.ttf" DESTINATION "${INSTALL_RESOURCE_DIR}/assets/fonts")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/themes" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(FILES "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-icon.png" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(FILES "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-splash-logo.png" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(FILES "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-splash-logo-dark.png" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(FILES "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-splash-loading.png" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(FILES "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/lichtfeld-splash-loading-dark.png" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(FILES "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/core11-logo.png" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(FILES "${CMAKE_SOURCE_DIR}/src/visualizer/gui/assets/core11-logo-dark.png" DESTINATION "${INSTALL_RESOURCE_DIR}/assets")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/visualizer/gui/rmlui/resources/" DESTINATION "${INSTALL_RESOURCE_DIR}/assets/rmlui")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/visualizer/gui/resources/locales" DESTINATION "${INSTALL_RESOURCE_DIR}")

# Plugin development documentation
set(DOC_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/doc/LichtFeld-Studio")
install(FILES
 "${CMAKE_SOURCE_DIR}/docs/PYTHON_API.md"
 "${CMAKE_SOURCE_DIR}/docs/plugin-system.md"
 "${CMAKE_SOURCE_DIR}/docs/plugin-use-cases.md"
 "${CMAKE_SOURCE_DIR}/docs/plugin-dev-workflow.md"
 DESTINATION "${DOC_INSTALL_DIR}"
 OPTIONAL
)

if(TARGET nvimgcodec)
 install(TARGETS nvimgcodec
 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
 )
 if(TARGET nvjpeg_ext)
 install(TARGETS nvjpeg_ext
 LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/extensions"
 RUNTIME DESTINATION "extensions"
 )
 endif()
 if(TARGET nvjpeg2k_ext)
 install(TARGETS nvjpeg2k_ext
 LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/extensions"
 RUNTIME DESTINATION "extensions"
 )
 endif()
endif()

# Portable build: bundle runtime dependencies via GET_RUNTIME_DEPENDENCIES
if(BUILD_PORTABLE)
 # Exclusion patterns for system libraries
 # nvcuda.dll is provided by the NVIDIA GPU driver, not the CUDA Toolkit
 set(_PRE_EXCLUDE [[api-ms-.*]] [[ext-ms-.*]] [[API-MS-.*]] [[EXT-MS-.*]] [[nvcuda[.]dll]])
 set(_POST_EXCLUDE
 [[.*[Ss]ystem32.*]] [[.*[Ww]indows.*]]
 [[/lib/x86_64-linux-gnu/.*]] [[/lib64/ld-linux.*]] [[/usr/lib.*]]
 [[.*ld-linux.*]] [[.*libc[.]so[.].*]] [[.*libpthread.*]]
 [[.*libdl[.]so.*]] [[.*libm[.]so[.].*]] [[.*librt[.]so.*]]
 [[.*libstdc[+][+].*]] [[.*libgcc_s.*]]
 [[.*libnvimgcodec.*]] [[.*libnvjpeg_ext.*]])

 # Search directories for runtime dependencies (quoted for paths with spaces on Windows)
 set(_SEARCH_DIRS_STR "\"${CMAKE_BINARY_DIR}\" \"${CMAKE_BINARY_DIR}/external/nvImageCodec\" \"${MUSAToolkit_BIN_DIR}\" \"${MUSAToolkit_LIBRARY_DIR}\"")

 # Helper: bundle runtime dependencies for a target
 function(bundle_runtime_deps TARGET_FILE IS_LIBRARY)
 if(IS_LIBRARY)
 set(_type "LIBRARIES")
 else()
 set(_type "EXECUTABLES")
 endif()
 install(CODE "
 set(_lib \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")
 file(GET_RUNTIME_DEPENDENCIES
 ${_type} \"${TARGET_FILE}\"
 RESOLVED_DEPENDENCIES_VAR _deps
 DIRECTORIES ${_SEARCH_DIRS_STR}
 PRE_EXCLUDE_REGEXES ${_PRE_EXCLUDE}
 POST_EXCLUDE_REGEXES ${_POST_EXCLUDE})
 foreach(_d IN LISTS _deps)
 file(INSTALL \"\${_d}\" DESTINATION \"\${_lib}\" FOLLOW_SYMLINK_CHAIN)
 endforeach()
 " COMPONENT runtime)
 endfunction()

 bundle_runtime_deps("$<TARGET_FILE:${PROJECT_NAME}>" FALSE)
 if(TARGET nvjpeg_ext)
 bundle_runtime_deps("$<TARGET_FILE:nvjpeg_ext>" TRUE)
 endif()

 if(UNIX AND NOT APPLE)
 configure_file(
 "${CMAKE_SOURCE_DIR}/cmake/templates/run_lichtfeld.sh.in"
 "${CMAKE_BINARY_DIR}/run_lichtfeld.sh"
 @ONLY)
 install(PROGRAMS "${CMAKE_BINARY_DIR}/run_lichtfeld.sh" DESTINATION ${CMAKE_INSTALL_BINDIR})
 elseif(WIN32)
 # Copy DLLs to bin/ for Windows search path
 install(CODE "
 # Copy all DLLs from lib/ to bin/
 file(GLOB _dlls \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/*.dll\")
 foreach(_d IN LISTS _dlls)
 file(INSTALL \"\${_d}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
 endforeach()
 # Copy extensions/ from build to dist/extensions/
 # Handle both single-config (Ninja) and multi-config (VS) generators
 foreach(_cfg IN ITEMS \"\" \"Release\" \"Debug\" \"RelWithDebInfo\" \"MinSizeRel\")
 if(_cfg)
 set(_ext_dir \"${CMAKE_BINARY_DIR}/\${_cfg}/extensions\")
 else()
 set(_ext_dir \"${CMAKE_BINARY_DIR}/extensions\")
 endif()
 if(EXISTS \"\${_ext_dir}\")
 # Install only DLLs from the build extensions directory to avoid non-binary files
 file(GLOB _ext_dlls \"\${_ext_dir}/*.dll\")
 if(_ext_dlls)
 foreach(_e IN LISTS _ext_dlls)
 file(INSTALL \"\${_e}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/extensions\")
 endforeach()
 endif()
 break()
 endif()
 endforeach()
 # Copy vcpkg DLLs
 if(EXISTS \"${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin\")
 file(GLOB _vcpkg \"${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin/*.dll\")
 foreach(_v IN LISTS _vcpkg)
 file(INSTALL \"\${_v}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
 endforeach()
 endif()
 " COMPONENT runtime)
 endif()
endif()

# Build statistics (parses .ninja_log, no Python required)
if(CMAKE_GENERATOR MATCHES "Ninja")
 # Manual target: ninja build_stats
 add_custom_target(build_stats
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/BuildStats.cmake
 WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
 COMMENT "Build time statistics by module"
 )

 # Auto-show after build: cmake -DSHOW_BUILD_STATS=ON ..
 option(SHOW_BUILD_STATS "Show build stats after every build" OFF)
 if(SHOW_BUILD_STATS)
 add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
 COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/BuildStats.cmake
 WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
 )
 endif()
endif()

# Clean build target: ninja clean_all (clears build + compiler cache)
if(CCACHE_PROGRAM)
 add_custom_target(clean_all
 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
 COMMAND ${CCACHE_PROGRAM} --clear
 COMMENT "Cleaning build and compiler cache (ccache)"
 )
elseif(SCCACHE_PROGRAM)
 add_custom_target(clean_all
 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
 COMMAND ${SCCACHE_PROGRAM} --zero-stats
 COMMENT "Cleaning build and resetting sccache stats"
 )
endif()
