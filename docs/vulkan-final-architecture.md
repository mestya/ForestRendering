# Vulkan Final Integration Architecture

This document defines the target architecture for the final merge:
- directory layout
- stable interfaces between shared core and feature passes
- render order (frame orchestration)

Goal: two developers can work in parallel with minimal conflicts, and the project can be merged into one runnable Vulkan renderer by integrating passes through a stable orchestration layer.

## Directory Layout

Recommended target layout under `src/vk/`:

- `src/vk/core/`
- `src/vk/core/VkContext.*`
- `src/vk/core/VkSwapchain.*`
- `src/vk/core/VkFrameSync.*`
- `src/vk/core/VkDeviceHelpers.*` (optional)

- `src/vk/renderer/`
- `src/vk/renderer/VkRenderer.*` (frame orchestrator)
- `src/vk/renderer/FrameContext.*` (per-frame handles + transient allocators)
- `src/vk/renderer/RenderTargets.*` (named attachments registry)

- `src/vk/features/`
- `src/vk/features/geometry/GeometryPass.*`
- `src/vk/features/shadow/ShadowPass.*`
- `src/vk/features/gbuffer/GBufferPass.*`
- `src/vk/features/lighting/LightingPass.*`
- `src/vk/features/post/PostProcessPass.*` (SSAO/blur optional)
- `src/vk/features/ui/ImGuiPass.*`

- `src/vk/assets/`
- `src/vk/assets/shaders/` (SPIR-V outputs or build rules)

Notes:
- Shared core stays small: context + swapchain + sync + common helpers.
- Each feature pass owns its own pipelines, descriptors, and GPU resources.
- The orchestrator owns render order only and never implements feature logic.

## Stable Interfaces

### FrameContext

`FrameContext` is the only object passed to feature passes during recording.
It should contain:
- `cmd` (command buffer)
- `frameIndex` (0..N-1)
- `imageIndex` (swapchain image)
- `swapchainExtent`
- `swapchainFormat`

Optional:
- transient descriptor allocator
- transient upload ring buffer
- per-frame uniform arena

### Pass Plugin Interface

Each feature pass implements the same lifecycle:

- `Create(VkContext& ctx, VkSwapchain const& sc, RenderTargets& rt)`
- `Destroy(VkContext& ctx)`
- `OnSwapchainRecreated(VkContext& ctx, VkSwapchain const& sc, RenderTargets& rt)`
- `Record(FrameContext& frame, RenderTargets& rt)`

Rules:
- `Create()` allocates long-lived resources (pipelines, descriptor set layouts, static buffers).
- `OnSwapchainRecreated()` recreates swapchain-dependent resources only.
- `Record()` only records commands; no hidden resource recreation.

### Shared Attachments Contract (RenderTargets)

To interoperate without tight coupling, define a small attachment registry owned by `VkRenderer`.
Passes read/write through named slots only.

Minimum recommended slots:
- Shadow output: `shadowDepth`
- GBuffer outputs: `gNormal`, `gAlbedo`, `depth` (position optional)
- SSAO outputs (optional): `ssaoRaw`, `ssaoBlur`
- Final color: either directly swapchain, or `hdrColor` then resolve

## Render Order (Frame Orchestration)

Target order inside `VkRenderer::DrawFrame()`:

1. Acquire swapchain image (core)
2. Begin command buffer (core)
3. ShadowPass.Record
4. GBufferPass.Record
5. PostProcessPass.Record (optional: SSAO/blur)
6. LightingPass.Record (writes to swapchain or intermediate)
7. ImGuiPass.Record (final overlay)
8. End command buffer (core)
9. Submit + present (core)

Swapchain recreation:
- Core detects out-of-date/suboptimal or resize.
- Core recreates swapchain.
- Renderer calls `OnSwapchainRecreated()` on all passes (deterministic order).

## Merge Strategy (Two Developers)

To avoid conflicts while still integrating to one executable:
- Shared core owner rotates weekly (only owner edits `src/vk/core/*` and `src/vk/renderer/*` that week).
- Feature owners do not edit each other's feature directories:
  - Dev A: `src/vk/features/geometry/*`, `src/vk/features/shadow/*`
  - Dev B: `src/vk/features/gbuffer/*`, `src/vk/features/lighting/*`, `src/vk/features/post/*`, `src/vk/features/ui/*`
- Any interface change goes first as a small PR:
  - update `FrameContext` and `RenderTargets`
  - update orchestrator call order
  - then feature PRs follow without large rebases

## Integration Done

By the final merge:
- Both developers' passes are callable from `VkRenderer::DrawFrame()`.
- Swapchain recreation works without manual steps.
- Validation has no high-severity errors in the standard path.
- This render order and interface contract stays the single source of truth.
