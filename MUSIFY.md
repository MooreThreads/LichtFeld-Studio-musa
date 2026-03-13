# MUSIFY.md - LichtFeld-Studio CUDA-to-MUSA Migration

## Project Info
- **Repository**: https://github.com/MrNeRF/LichtFeld-Studio.git
- **Type**: App (Standalone C++ Application)
- **Build System**: CMake 3.30+ with vcpkg
- **MUSA SDK**: 4.3.5
- **Target GPU**: MTT S5000

## Migration Plan

### Stage 0: SDK Version Resolution
- [x] Resolved MUSA SDK: 4.3.5
- [x] Docker image: `sh-harbor.mthreads.com/mcctest/musa-train:4.3.5_kuae2.1_20260119_torch2.7.1_ubuntu`
- [x] Container: `musify-lichtfeld`

### Stage 1: Code Analysis
- [x] CUDA files: 60 .cu + 33 .cuh
- [x] 91+ files with CUDA runtime API calls
- [x] Libraries: cuRAND, CUB, Thrust, cooperative_groups, cuda_fp16
- [x] CUDA-GL interop present but conditionally compiled
- [x] nvImageCodec (NVIDIA-specific) present

### Stage 2: API Mapping
- cuda_runtime.h -> musa_runtime.h
- cuda_fp16.h -> musa_fp16.h
- cuda_gl_interop.h -> musa_gl_interop.h
- curand -> murand
- .cu -> .mu
- CUDA:: -> MUSA:: (CMake targets)
- CUDAToolkit -> MUSAToolkit

### Stage 3: Port -> Build -> Fix

#### Porting Steps
1. [ ] Run SimplePorting on .cu/.cuh files to rename to .mu/.muh
2. [ ] Modify CMakeLists.txt files to use MUSA instead of CUDA
3. [ ] Handle nvImageCodec (disable or stub)
4. [ ] Update compiler flags (nvcc -> mcc)
5. [ ] Build and fix errors iteratively

## Known Issues
- CMake 3.30 required but container has 3.25.2 (need to install newer cmake)
- vcpkg dependencies need to be installed in container
- nvImageCodec and nvjpeg/nvjpeg2k are NVIDIA-specific (will need to be disabled)
- The project is very large with many external dependencies

## Knowledge Base
(append-only, survives agent restarts)

### Decision Log
- 2026-03-13: Selected MUSA SDK 4.3.5 (latest available)
- 2026-03-13: Will disable nvImageCodec (no MUSA equivalent for nvjpeg)
- 2026-03-13: CUDA-GL interop has MUSA equivalent (musa_gl_interop.h) - will attempt migration
- 2026-03-13: All critical CUDA sub-libraries available in MUSA SDK (CUB, Thrust, cooperative_groups, fp16)
