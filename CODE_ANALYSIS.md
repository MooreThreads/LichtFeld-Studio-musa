# LichtFeld-Studio CUDA Code Analysis

## Project Classification
- **Type**: App (Standalone C++ Application with CMake)
- **Build System**: CMake 3.30+ with vcpkg
- **Languages**: C++23, CUDA 20 (requires CUDA Toolkit 12.8+)
- **Artifact**: Executable `LichtFeld-Studio` + several static libraries

## Project Overview
LichtFeld Studio is a high-performance 3D Gaussian Splatting implementation in C++ and CUDA. It provides training, rendering, and real-time visualization capabilities for neural rendering. The project builds a standalone executable with a GUI (SDL3 + OpenGL + ImGui).

## CUDA File Inventory

### Total CUDA Files
- `.cu` files: 60
- `.cuh` files: 33
- Files with CUDA runtime API calls: 91+ files (including .cpp/.hpp)
- Total CUDA kernel occurrences: ~1443 across 73 files

### CUDA Files by Module

#### Core Module (`src/core/`)
- `cuda/memory_arena.cu` - GPU memory arena allocator
- `cuda/lanczos_resize/lanczos_resize.cu` - Lanczos image resize (uses cooperative_groups)
- `cuda/undistort/undistort.cu` - Camera undistortion
- `cuda/kernels/kmeans_new.cu` - K-means clustering (uses thrust)
- `cuda/kernels/kdtree_kmeans.cu` - KD-tree k-means (uses thrust)
- `cuda/kernels/morton_encoding_new.cu` - Morton encoding (uses thrust)
- `cuda/kernels/selection_ops.cu` - Selection operations (uses thrust)
- `cuda/tensor_debug.cu` - Tensor debugging utilities

#### Tensor Module (`src/core/tensor/`)
- `tensor_ops.cu` - Basic tensor operations (uses cub)
- `tensor_warp_reduce.cu` - Warp-level reductions
- `tensor_matrix_ops.cu` - Matrix operations
- `tensor_broadcast_ops.cu` - Broadcast operations
- `tensor_masking_ops.cu` - Masking operations (uses cub + thrust extensively)
- `tensor_random_ops.cu` - Random operations (uses curand + thrust)
- `tensor_strided_ops.cu` - Strided operations
- `tensor_dot_optimized.cu` - Optimized dot product
- `tensor_nn_ops.cu` - Neural network operations
- `tensor_fused_pointwise.cu` - Fused pointwise operations
- `tensor_exports.cu` - Tensor export (uses cuda_fp16)

#### Training Module (`src/training/`)
- `kernels/ssim.cu` - SSIM loss computation
- `kernels/l1_loss.cu` - L1 loss
- `kernels/ssim_reduction.cu` - SSIM reduction
- `kernels/regularization.cu` - Regularization kernels
- `kernels/bilateral_grid_forward.cu` - Bilateral grid forward pass
- `kernels/bilateral_grid_backward.cu` - Bilateral grid backward pass
- `kernels/bilateral_grid_tv.cu` - Bilateral grid total variation
- `kernels/ppisp.cu` - PPISP kernels (uses cub)
- `kernels/mcmc_kernels.cu` - MCMC kernels (uses curand + cub)
- `kernels/densification_kernels.cu` - Densification
- `kernels/grad_alpha.cu` - Gradient alpha
- `kernels/image_kernels.cu` - Image processing kernels
- `components/sparsity_optimizer_kernels.cu` - Sparsity optimization
- `components/ppisp_controller.cu` - PPISP controller
- `components/ppisp_controller_pool.cu` - PPISP controller pool

#### Rasterization Backends
**gsplat** (`src/training/rasterization/gsplat/`):
- `IntersectTile.cu`, `ProjectionUT3DGSFused.cu`, `RasterizeToPixelsFromWorld3DGSFwd.cu`
- `RasterizeToPixelsFromWorld3DGSBwd.cu`, `QuatToRotmatCUDA.cu`, `RelocationCUDA.cu`
- `SphericalHarmonicsCUDA.cu`

**fastgs** (`src/training/rasterization/fastgs/`):
- `rasterization/src/forward.cu`, `rasterization/src/backward.cu`, `rasterization/src/rasterization_api.cu`
- `optimizer/src/adam.cu`, `optimizer/src/adam_api.cu`

**edge_compute** (`src/training/rasterization/edge_compute/`):
- `rasterization/src/forward.cu`, `rasterization/src/rasterization_api.cu`

#### Rendering Module (`src/rendering/`)
- `cuda_gl_kernels.cu` - CUDA-OpenGL interop kernels
- `rasterizer/gsplat_fwd/` - Forward-only gsplat (mirrors training gsplat)
- `rasterizer/rasterization/` - Forward rasterization

#### I/O Module (`src/io/`)
- `cuda/image_format_kernels.cu` - Image format conversion
- `cuda/morton_encoding.cu` - Morton encoding
- `cuda/kmeans.cu` - K-means for I/O
- `video/color_convert.cu` - Video color conversion

#### External (`external/nvImageCodec/`)
- `src/imgproc/convert_kernel_gpu.cu` - GPU image conversion

## CUDA API Usage Summary

### Runtime APIs Used
- `cuda_runtime.h` - Pervasive across all modules
- `cudaMalloc`, `cudaFree`, `cudaMemcpy`, `cudaMemset` - Memory management
- `cudaMemcpy2DToArray`, `cudaMemcpy2DFromArray` - 2D array operations
- `cudaStreamCreate`, `cudaStreamSynchronize`, `cudaDeviceSynchronize` - Stream management
- `cudaGetDevice`, `cudaSetDevice`, `cudaGetDeviceCount` - Device management
- `cudaEventCreate`, `cudaStreamWaitEvent` - Event management
- `cudaGetDeviceProperties` - Device query

### CUDA Libraries Used
| Library | Files | Notes |
|---------|-------|-------|
| **cuRAND** | 14 files | Random number generation (curandState, curand_init, curand_uniform) |
| **CUB** | 13 files | Block/warp reductions, scan, sort |
| **Thrust** | 20+ files | sort, copy, gather, scan, scatter, reduce, device_vector |
| **cooperative_groups** | 1 file | lanczos_resize.cu |
| **cuda_fp16** | 10 files | Half-precision support (__half, half2) |

### NOT Used
- cuBLAS (no matrix multiply libraries)
- cuSPARSE, cuSOLVER, cuFFT
- cuDNN, NCCL

### Advanced CUDA Features
- Warp intrinsics: `__shfl_xor_sync`, `__shfl_down_sync`, `__ballot_sync` (26 files)
- Atomic operations: `atomicAdd`, `atomicMin`, `atomicMax`, `atomicCAS` (26 files)
- Texture/Surface objects: `cudaSurfaceObject_t`, `surf2Dwrite` (in cuda_gl_kernels.cu)
- CUDA-OpenGL interop: `cudaGraphicsGLRegisterImage`, `cudaGraphicsMapResources` (conditionally compiled)

## External Dependencies

### CUDA-Specific
- **nvImageCodec** (in `external/`): GPU-accelerated image codec with its own .cu file
  - Depends on nvjpeg, nvjpeg2k (NVIDIA-specific libraries)

### Build Dependencies (via vcpkg)
- TBB, OpenGL, glad, SDL3, glm, imgui, implot
- nlohmann_json, args, spdlog, Freetype, WebP
- LibArchive, OpenImageIO, OpenSSL, assimp, RmlUi, Boost

### Embedded External Libraries
- OpenMesh (half-edge mesh data structure)
- ImGuizmo (gizmo controls)
- Zep (embeddable editor)
- libvterm (terminal emulator, git submodule)

## Migration Complexity Assessment

### High Complexity Factors
1. **Massive CUDA surface area**: 60+ .cu files, 33 .cuh headers, 91+ files with CUDA API calls
2. **CUDA-OpenGL interop**: Deep integration via `cuda_gl_interop.h` - MUSA may not support this
3. **C++23 requirement**: mcc may not support all C++23 features
4. **CUDA Toolkit 12.8 minimum**: Very recent features expected
5. **vcpkg dependency chain**: 20+ external packages need to be available
6. **nvImageCodec**: NVIDIA-specific GPU image codec
7. **nvjpeg/nvjpeg2k**: NVIDIA-specific image compression libraries
8. **Surface objects**: `cudaSurfaceObject_t` in rendering kernels

### Migration Blockers
1. **CMake 3.30 required** but MUSA SDK image has CMake 3.25.2
2. **CUDAToolkit 12.8 REQUIRED** in CMakeLists.txt - needs to be relaxed/replaced
3. **CUDA-GL interop** - may not be supported on MUSA/MT GPUs
4. **nvjpeg/nvjpeg2k** - NVIDIA-specific, no MUSA equivalent
5. **nvImageCodec** - NVIDIA-specific

### Recommended Migration Strategy
1. **Disable CUDA-GL interop** (`-DENABLE_CUDA_GL_INTEROP=OFF`) - fallback exists
2. **Disable/stub nvImageCodec** - requires NVIDIA-specific nvjpeg
3. **Replace `find_package(CUDAToolkit 12.8)` with MUSA detection**
4. **Rename .cu -> .mu, update headers** (cuda_runtime.h -> musa_runtime.h, etc.)
5. **Replace nvcc flags with mcc flags** in CMakeLists.txt
6. **Handle thrust/cub** - check MUSA SDK for compatibility
7. **Handle cooperative_groups** - check MUSA SDK support
8. **Handle cuda_fp16** - map to musa_fp16 equivalents
