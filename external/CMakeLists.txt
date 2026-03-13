# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
#
# SPDX-License-Identifier: GPL-3.0-or-later

# External dependencies (EXCLUDE_FROM_ALL to prevent third-party install rules)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/imguizmo/CMakeLists.txt")
 message(STATUS "Adding ImGuizmo")
 add_subdirectory(imguizmo EXCLUDE_FROM_ALL)
else()
 message(WARNING "ImGuizmo not found - gizmo controls unavailable")
endif()

# Zep - embeddable editor core
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/zep/CMakeLists.txt")
 message(STATUS "Adding Zep from external/zep")
 add_subdirectory(zep EXCLUDE_FROM_ALL)
else()
 message(WARNING "Zep not found at external/zep")
endif()

# OpenMesh - Half-edge mesh data structure
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/OpenMesh/CMakeLists.txt")
 message(STATUS "Adding OpenMesh")
 set(OPENMESH_BUILD_SHARED OFF CACHE BOOL "" FORCE)
 set(OPENMESH_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
 set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
 add_subdirectory(OpenMesh EXCLUDE_FROM_ALL)
else()
 message(WARNING "OpenMesh not found at external/OpenMesh")
endif()

# nvImageCodec - GPU-accelerated image codec library (NVIDIA-only, disabled for MUSA)
find_package(Boost REQUIRED)
message(STATUS "Skipping nvImageCodec (NVIDIA-specific, not available for MUSA)")
# nvImageCodec depends on nvjpeg/nvjpeg2k which are NVIDIA-proprietary.
# The IO module handles this gracefully: if(TARGET nvimgcodec) guards in src/io/CMakeLists.txt.
