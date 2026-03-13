# SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
#
# SPDX-License-Identifier: GPL-3.0-or-later

# MUSA library for IO-specific kernels (morton encoding, k-means)
set(LFS_IO_MU_SOURCES
 cuda/morton_encoding.mu
 cuda/kmeans.mu
 cuda/image_format_kernels.mu
)
set_source_files_properties(${LFS_IO_MU_SOURCES}
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")

add_library(lfs_io_cuda STATIC
 cuda/morton_encoding.hpp
 ${LFS_IO_MU_SOURCES}
 cuda/kmeans.hpp
 cuda/image_format_kernels.muh
)

target_include_directories(lfs_io_cuda
 PUBLIC
 ${CMAKE_CURRENT_SOURCE_DIR}
 PRIVATE
 ${CMAKE_SOURCE_DIR}/src/core/include
 ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lfs_io_cuda
 INTERFACE
 lfs_core # Propagate to consumers, not linked during device linking
 PRIVATE
 MUSA::musart
 fmt::fmt
 spdlog::spdlog
)

set_target_properties(lfs_io_cuda PROPERTIES

 POSITION_INDEPENDENT_CODE ON
)

# CUDA compiler options for lfs_io_cuda
target_compile_options(lfs_io_cuda PRIVATE
 # CUDA device code + MSVC host compiler flags (Windows only)
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:-O0 -g -8 >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:-O3 -8 >

 # CUDA device code for non-Windows
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0 -g >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3 >
)

if(MSVC)
 target_compile_definitions(lfs_io_cuda PRIVATE
 $<$<COMPILE_LANGUAGE:CXX>:FMT_UNICODE=0>
 )
endif()

# SPZ library (Niantic compressed format) - MIT licensed
add_library(spz_lib STATIC
 ${CMAKE_SOURCE_DIR}/external/spz/load-spz.cc
 ${CMAKE_SOURCE_DIR}/external/spz/splat-types.cc
 ${CMAKE_SOURCE_DIR}/external/spz/splat-c-types.cc
)
target_include_directories(spz_lib PUBLIC ${CMAKE_SOURCE_DIR}/external/spz)
target_link_libraries(spz_lib PRIVATE ZLIB::ZLIB)
target_compile_features(spz_lib PUBLIC cxx_std_17)
set_target_properties(spz_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Video encoding library (FFmpeg + CUDA for GPU-accelerated encoding)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
 libavcodec
 libavformat
 libswscale
 libavutil
)

set(LFS_VIDEO_MU_SOURCES video/color_convert.mu)
set_source_files_properties(${LFS_VIDEO_MU_SOURCES}
    PROPERTIES LANGUAGE CXX COMPILE_FLAGS "-x musa")

add_library(lfs_video STATIC
 include/io/video/video_encoder_interface.hpp
 include/io/video/video_export_options.hpp
 video/video_encoder.hpp
 video/video_encoder.cpp
 video/color_convert.muh
 ${LFS_VIDEO_MU_SOURCES}
 video_frame_extractor.hpp
 video_frame_extractor.cpp
 video_player.hpp
 video_player.cpp
)

target_include_directories(lfs_video
 PUBLIC
 $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
 $<INSTALL_INTERFACE:include>
 PRIVATE
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/src/core/include
 ${CMAKE_SOURCE_DIR}/src
 ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(lfs_video
 PRIVATE
 PkgConfig::FFMPEG
 spdlog::spdlog
 MUSA::musart
)

target_compile_features(lfs_video PUBLIC cxx_std_23)

set_target_properties(lfs_video PROPERTIES

 POSITION_INDEPENDENT_CODE ON
)

target_compile_options(lfs_video PRIVATE
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:-O0 -g -8>
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:-O3 -8>
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:-O0 -g >
 $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O3 >
)

# Main IO library
add_library(lfs_io STATIC
 # Implementation file only (header is in include/io/)
 loader.cpp

 # Internal implementation files
 # Core interfaces and types
 loader_interface.hpp
 loader_registry.hpp
 loader_service.hpp
 loader_service.cpp

 # Format implementations (load + save)
 formats/ply.hpp
 formats/ply.cpp
 formats/colmap.hpp
 formats/colmap.cpp
 formats/transforms.hpp
 formats/transforms.cpp
 formats/sogs.hpp
 formats/sogs.cpp
 formats/html.hpp
 formats/html.cpp
 formats/html_viewer_resources.hpp
 formats/spz.hpp
 formats/spz.cpp

 # Concrete loader implementations
 loaders/ply_loader.hpp
 loaders/ply_loader.cpp
 loaders/colmap_loader.hpp
 loaders/colmap_loader.cpp
 loaders/blender_loader.hpp
 loaders/blender_loader.cpp
 loaders/sogs_loader.hpp
 loaders/sogs_loader.cpp
 loaders/spz_loader.hpp
 loaders/spz_loader.cpp
 loaders/checkpoint_loader.hpp
 loaders/checkpoint_loader.cpp
 loaders/mesh_loader.hpp
 loaders/mesh_loader.cpp
 mesh/openmesh_bridge.hpp
 mesh/openmesh_bridge.cpp
 mesh/texture_loader.hpp
 mesh/texture_loader.cpp
 cache_image_loader.cpp
 pipelined_image_loader.cpp
 stb_image_impl.cpp
 tinyply.cpp

)

# Set include directories
target_include_directories(lfs_io
 PUBLIC
 $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
 $<INSTALL_INTERFACE:include>
 PRIVATE
 ${CMAKE_SOURCE_DIR}/src
 ${CMAKE_SOURCE_DIR}/external
 ${CMAKE_SOURCE_DIR}/external/spz
)

# Find the actual WebP library targets
if(TARGET WebP::webpdecoder)
 set(WEBP_DECODE_LIB WebP::webpdecoder)
elseif(TARGET WebP::webp)
 set(WEBP_DECODE_LIB WebP::webp)
elseif(TARGET webpdecoder)
 set(WEBP_DECODE_LIB webpdecoder)
else()
 find_library(WEBP_DECODE_LIBRARY NAMES webpdecoder webp libwebpdecoder libwebp REQUIRED)
 set(WEBP_DECODE_LIB ${WEBP_DECODE_LIBRARY})
endif()

# WebP encoder for SOG/HTML export
if(TARGET WebP::webp)
 set(WEBP_ENCODE_LIB WebP::webp)
elseif(TARGET webp)
 set(WEBP_ENCODE_LIB webp)
else()
 find_library(WEBP_ENCODE_LIBRARY NAMES webp libwebp REQUIRED)
 set(WEBP_ENCODE_LIB ${WEBP_ENCODE_LIBRARY})
endif()

# Find the actual LibArchive library target
if(TARGET LibArchive::LibArchive)
 set(ARCHIVE_LIB LibArchive::LibArchive)
elseif(TARGET archive)
 set(ARCHIVE_LIB archive)
elseif(TARGET archive_static)
 set(ARCHIVE_LIB archive_static)
else()
 find_library(ARCHIVE_LIBRARY NAMES archive libarchive REQUIRED)
 set(ARCHIVE_LIB ${ARCHIVE_LIBRARY})
endif()

# Ensure CUDA library is built first
add_dependencies(lfs_io lfs_io_cuda)

# Link dependencies
target_link_libraries(lfs_io
 PUBLIC
 lfs_core # Link to modular core library for SplatData, Camera, etc.
 PRIVATE
 TBB::tbb
 spdlog::spdlog
 nlohmann_json::nlohmann_json
 ${WEBP_DECODE_LIB} # WebP for SOG decompression
 ${WEBP_ENCODE_LIB} # WebP for SOG/HTML export
 ${ARCHIVE_LIB} # LibArchive for SOG bundle support
 MUSA::musart # CUDA runtime for nvImageCodec
 lfs_io_cuda # IO-specific CUDA kernels (morton, k-means)
 lfs_core_musa # Core CUDA utilities (lanczos_resize for nvcodec)
 assimp::assimp # Multi-format mesh import
 $<IF:$<PLATFORM_ID:Windows>,OpenMeshCore,OpenMeshCoreStatic>
 spz_lib # Niantic SPZ compressed format
)

# Link nvImageCodec if available
if(TARGET nvimgcodec)
 target_link_libraries(lfs_io PRIVATE nvimgcodec)
 add_dependencies(lfs_io nvimgcodec)
 message(STATUS "Linking nvImageCodec to lfs_io")
else()
 message(WARNING "nvImageCodec target not found - JPEG hardware decode will not work")
endif()

# Set C++ standard
target_compile_features(lfs_io PUBLIC cxx_std_23)

# Enable position-independent code for shared library linking (Python module)
set_target_properties(lfs_io PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Compiler options
target_compile_options(lfs_io PRIVATE
 $<$<CXX_COMPILER_FRONTEND_VARIANT:GNU>:-Wall -Wextra -Wpedantic>
 $<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/W4>
)

# AVX2 support for ply.cpp
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
 include(CheckCXXCompilerFlag)
 check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)

 if (COMPILER_SUPPORTS_AVX2)
 target_compile_options(lfs_io PRIVATE
 $<$<COMPILE_LANGUAGE:CXX>:-mavx2 -mfma>
 )
 target_compile_definitions(lfs_io PRIVATE HAS_AVX2_SUPPORT)
 endif ()
endif ()

if(NOT BUILD_PORTABLE)
 install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/io
 DESTINATION include)
endif()

message(STATUS "I/O module configured with SOG support")
message(STATUS " IO CUDA library: lfs_io_cuda (morton, k-means)")
message(STATUS " WebP decoder library: ${WEBP_DECODE_LIB}")
message(STATUS " Archive library: ${ARCHIVE_LIB}")
