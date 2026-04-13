# Vulkan File Implementation Map (`src/vk/`)

This is a per-file checklist of what each file is responsible for implementing.
It matches the final merge architecture described in `docs/vulkan-final-architecture.md`.

## Core (`src/vk/core/`)

`src/vk/core/VkContext.hpp`
- Own Vulkan handles and lifetime: instance, debug messenger, surface, physical device, device, queues.
- Provide accessors for device/queues/helpers used by passes.
- Own global configuration: validation on/off, frames-in-flight count.

`src/vk/core/VkContext.cpp`
- Implement `Init()` and `Shutdown()`:
- Create instance + required extensions/layers.
- Create surface (via GLFW).
- Select physical device and create logical device + queues.
- Set up debug messenger (if enabled).

`src/vk/core/VkSwapchain.hpp`
- Describe swapchain state: extent, format, image count.
- Store swapchain images + views.
- Provide acquire/present helpers (or expose raw handles for core to present).

`src/vk/core/VkSwapchain.cpp`
- Create/recreate swapchain:
- Choose surface format, present mode, extent.
- Create image views.
- Destroy old swapchain resources safely.

`src/vk/core/VkFrameSync.hpp`
- Encapsulate per-frame semaphores/fences:
- imageAvailable, renderFinished, inFlightFence per frame.
- Expose wait/reset helpers used by the frame loop.

`src/vk/core/VkFrameSync.cpp`
- Create and destroy sync primitives.
- Implement `BeginFrame()`/`EndFrame()` style helpers (optional) or just utilities.

## Renderer (`src/vk/renderer/`)

`src/vk/renderer/FrameContext.hpp`
- Define the minimal per-frame data passed to passes:
- Command buffer handle reference.
- Frame index + swapchain image index.
- Swapchain extent/format.
- Optional transient allocators for descriptors/uploads.

`src/vk/renderer/RenderTargets.hpp`
- Define the shared attachment registry between passes:
- Named images/views for:
  - shadow depth
  - gbuffer attachments (normal/albedo/depth, position optional)
  - ssao outputs (optional)
  - intermediate color target (optional)
- Ownership rules:
  - which pass creates which target
  - who reads/writes each target

`src/vk/renderer/IRenderPass.hpp`
- Stable pass interface all features must implement:
  - `Create()`, `Destroy()`, `OnSwapchainRecreated()`, `Record()`
- No Vulkan logic here; only the contract.

`src/vk/renderer/VkRenderer.hpp`
- Orchestrator that:
  - owns `RenderTargets`
  - holds pass list in deterministic order
  - calls pass lifecycle functions
  - exposes `DrawFrame()` to the main loop

`src/vk/renderer/VkRenderer.cpp`
- Implement:
  - `Create()` (call Create on passes)
  - `OnSwapchainRecreated()` (call on passes)
  - `DrawFrame()` (call Record on passes in order)
- Keep this file free of feature-specific logic.

## Features (`src/vk/features/`)

General rule for all passes:
- Each pass owns its pipelines, descriptor set layouts, descriptor sets, samplers, and its own GPU images/buffers.
- Each pass only communicates through `FrameContext` and `RenderTargets`.

### Geometry (`src/vk/features/geometry/`)

`src/vk/features/geometry/GeometryPass.hpp`
- Declare the geometry pass:
  - static mesh pipeline(s)
  - instancing support (later)
  - depth state (writes depth)

`src/vk/features/geometry/GeometryPass.cpp`
- Create:
  - pipeline layout + pipeline
  - descriptor sets (camera matrices, material textures)
  - vertex/index buffers (later moved to an asset system)
- Record:
  - bind pipeline/descriptors
  - bind vertex/index buffers
  - draw terrain/trees/grass (start with one mesh)

### Shadow (`src/vk/features/shadow/`)

`src/vk/features/shadow/ShadowPass.hpp`
- Declare shadow pass:
  - shadow depth image target (2D or array)
  - depth-only pipeline

`src/vk/features/shadow/ShadowPass.cpp`
- Create:
  - shadow depth image + view + sampler (if sampled)
  - pipeline for depth rendering
- Record:
  - render depth from light POV into `RenderTargets.shadowDepth`

### GBuffer (`src/vk/features/gbuffer/`)

`src/vk/features/gbuffer/GBufferPass.hpp`
- Declare GBuffer pass:
  - attachments (normal/albedo/depth; position optional)
  - pipeline for writing gbuffer

`src/vk/features/gbuffer/GBufferPass.cpp`
- Create:
  - gbuffer images/views sized to swapchain
  - pipeline layout + pipeline
- OnSwapchainRecreated:
  - recreate sized attachments
- Record:
  - render scene geometry writing to gbuffer targets in `RenderTargets`

### Lighting (`src/vk/features/lighting/`)

`src/vk/features/lighting/LightingPass.hpp`
- Declare lighting pass:
  - full-screen pipeline
  - descriptor set reading gbuffer + shadow + ssao (optional)

`src/vk/features/lighting/LightingPass.cpp`
- Create:
  - descriptor set layout for inputs
  - full-screen pipeline
- Record:
  - sample gbuffer/shadow and write final color to swapchain (or intermediate)

### Post (`src/vk/features/post/`)

`src/vk/features/post/PostProcessPass.hpp`
- Declare post-processing pass:
  - SSAO + blur (optional, can be stubbed)

`src/vk/features/post/PostProcessPass.cpp`
- Create:
  - SSAO images/views sized to swapchain (if enabled)
  - pipelines for SSAO + blur
- Record:
  - compute SSAO from gbuffer, blur, write into `RenderTargets.ssao*`

### UI (`src/vk/features/ui/`)

`src/vk/features/ui/ImGuiPass.hpp`
- Declare ImGui pass:
  - init/shutdown of ImGui Vulkan backend
  - descriptor pool for ImGui

`src/vk/features/ui/ImGuiPass.cpp`
- Create:
  - `ImGui::CreateContext`, `ImGui_ImplGlfw_InitForVulkan`, `ImGui_ImplVulkan_Init`
  - upload fonts
- Record:
  - begin/end ImGui frame
  - record draw data into the frame command buffer as the final overlay
