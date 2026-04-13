# ForestSceneVK (Vulkan) - Framework Notes

This branch adds a minimal Vulkan executable target: `ForestSceneVK`.

For module boundaries / collaboration guidelines, see `VULKAN_ARCHITECTURE.md`.

## Quick start

### Configure

```bash
cmake -S . -B build_vk -DFORESTSCENE_ENABLE_VULKAN=ON
```

### Build

```bash
cmake --build build_vk --target ForestSceneVK
```

### Run

```bash
./build_vk/ForestSceneVK
```

If you are using a multi-config generator (Visual Studio / Xcode), the binary
will be under the configuration folder (e.g. `build_vk/Release/ForestSceneVK`).

## VSCode (CMake Tools)

This repo includes a minimal `.vscode/` setup:

- Configure uses build dir `build_vk_vscode` and enables Vulkan.
- Run/Debug uses the CMake Tools launch target and a pre-launch build task.

In VSCode:

1. Select a kit/toolchain (Command Palette: `CMake: Select a Kit`)
2. Configure (`CMake: Configure`)
3. Build the target `ForestSceneVK`
4. Press `F5` and choose **Run ForestSceneVK (CMake Tools)**

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
