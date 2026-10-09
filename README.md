# LichtFeld-Studio-musa

`LichtFeld-Studio-musa` is the Moore Threads MUSA maintenance line of
[LichtFeld-Studio](https://github.com/MrNeRF/LichtFeld-Studio), an application
for 3D Gaussian Splatting reconstruction and interactive visualization.

## Source and version

- Upstream project: MrNeRF LichtFeld-Studio
- Inherited source identifier: `f7620d99` (the migration ref has no release tag)
- Upstream baseline: `f24c25b7da6ce4c7528cd753cab27a38de98c16b`
- Maintainer: Moore Threads

The MUSA line adds MUSA compiler and runtime integration for the CUDA kernels
and preserves the inherited application, external components, tests, and CMake
layout. The source identifier is used for the publication branch because the
upstream line does not declare a release version.

## Requirements and build

Use CMake 3.30 or newer, a C++ compiler, vcpkg dependencies, and the MUSA SDK
when selecting the MUSA backend. A typical configure/build outline is:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Follow the repository's vcpkg and platform configuration for the selected
deployment. MUSA and graphics toolkits are external prerequisites.

## Layout and contribution

Application code is under `src/` and `include/`; `external/`, `resources/`,
`docs/`, and CMake files retain the upstream organization. Keep MUSA changes
separate from inherited backends and preserve third-party notices.

Moore Threads maintains this source line. See [LICENSE](LICENSE) for the
applicable declaration.
