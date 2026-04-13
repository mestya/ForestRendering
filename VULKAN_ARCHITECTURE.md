# Vulkan migration plan (modules + collaboration)

Goal: migrate the original OpenGL project into a Vulkan renderer while keeping
team work parallel and low-conflict (macOS + Windows).

## Layered modules (recommended boundaries)

### Core / shared (touch less, reviewed by both)

- **Platform**
  - Window creation (GLFW), input/events, resize handling, main loop.
- **VkContext**
  - Instance/device/queues/surface/swapchain, command pools, frame sync.
  - Minimal API: `init(window)`, `draw_frame()`, `recreate_swapchain()`, `cleanup()`.
- **Resources**
  - Buffer/Image creation + views/samplers, staging uploads, lifetime tracking.
  - Recommendation: introduce VMA later to simplify memory management.
- **Shaders & Pipelines**
  - SPIR-V compilation pipeline (offline or build step), pipeline cache.
  - Descriptor set layout + pipeline layout conventions.
- **Assets**
  - Model/texture loading (assimp + stb), material representation.
- **Scene**
  - Transform, camera, drawable list, culling (optional initially).
- **Debug/Tools**
  - Validation layers, debug labels, GPU timers, debug views.

### Rendering feature blocks (ownable by one person)

Each block should live under `src/vulkan/stages/<FeatureName>/...` and only
expose a small interface so other blocks can call it without sharing internals.

- **ShadowMap**
  - Depth-only pass, light matrices, shadow sampling helpers.
- **GBuffer (Deferred)**
  - MRT attachments and geometry pass (albedo/normal/roughness/etc).
- **Lighting (Deferred)**
  - Fullscreen/compute lighting using GBuffer (+ shadows).
- **Forward / Transparent**
  - Alpha blended objects, special materials (e.g. foliage).
- **SSAO**
  - Screen-space occlusion (depends on depth/normal).
- **PostFX**
  - Bloom / tonemap / gamma / TAA (later).
- **Sky**
  - Skybox / atmosphere (optional).
- **DebugViews**
  - Visualize shadow map / attachments / normals / depth.

## Repository layout (current + intended)

- `ForestSceneVK` entry: `src/main_vk.cpp`
- Vulkan runtime:
  - `src/vulkan/App.*` (window + main loop)
  - `src/vulkan/Context.*` (Vulkan context + swapchain + frame loop)
  - `src/vulkan/stages/` (feature blocks)

Suggestion for future structure:

- `src/vulkan/core/` (Context, device helpers, swapchain, sync)
- `src/vulkan/resources/` (buffers/images, uploads, descriptors)
- `src/vulkan/shaders/` (shader/pipeline helpers)
- `src/vulkan/stages/<Feature>/` (ShadowMap, GBuffer, Lighting, PostFX, ...)

## Collaboration rules (to avoid merge conflicts)

- One owner for **core/shared** modules; others contribute via PRs.
- One owner per **stage** directory; others treat it as an external API.
- Do not store generated build output in git (use ignored `build_*` dirs).
- Keep stage interfaces stable:
  - `init(device/context, size)`
  - `on_resize(size)`
  - `record(cmd, frame_index, inputs, outputs)`

## Milestones (practical order)

1. **Triangle / ImGui overlay (sanity)**
2. **Resource upload path** (staging buffers + texture upload)
3. **Mesh draw path** (vertex/index buffers, materials)
4. **Depth prepass or ShadowMap**
5. **GBuffer pass**
6. **Deferred lighting pass**
7. **Transparency / foliage**
8. **SSAO + PostFX**

