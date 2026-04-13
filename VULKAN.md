# ForestSceneVK (Vulkan) - Framework Notes

This branch adds a minimal Vulkan executable target: `ForestSceneVK`.

## Quick start

### Configure

```bash
cmake -S . -B build -DFORESTSCENE_ENABLE_VULKAN=ON
```

### Build

```bash
cmake --build build --config Release
```

### Run

```bash
./build/ForestSceneVK
```

If you are using a multi-config generator (Visual Studio / Xcode), the binary
will be under the configuration folder (e.g. `build/Release/ForestSceneVK`).

## Vulkan SDK setup (team notes)

- Windows: install LunarG Vulkan SDK and ensure `VULKAN_SDK` is set (CMake's
  `find_package(Vulkan)` should succeed).
- macOS: install the Vulkan SDK (includes MoltenVK). If `find_package(Vulkan)`
  fails, verify that the Vulkan SDK environment is set up for your shell / IDE.

## Collaboration / feature blocks

Suggested directory convention (to keep work disjoint):

- `src/vulkan/stages/ShadowMap*` (shadow map pass + depth pipeline)
- `src/vulkan/stages/GBuffer*` (deferred G-buffer pass)
- `src/vulkan/stages/Lighting*` (deferred lighting/composite)
- `src/vulkan/stages/Post*` (SSAO/bloom/tonemap)

The current skeleton only clears the swapchain color each frame; it is meant as
the base for adding passes and resource management.

