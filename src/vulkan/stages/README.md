# Vulkan stages (placeholders)

This folder is intended for feature-blocked rendering stages (shadow map,
G-buffer, lighting, post-processing, etc.).

Current code path: `forest::vk::Context` records a command buffer that only
clears the swapchain image.

