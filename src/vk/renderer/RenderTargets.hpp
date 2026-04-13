#pragma once

namespace vkfw {

// Named attachment registry shared between passes.
// This is a skeleton; the real version will store Vulkan images/views.
struct RenderTargets {
  bool has_shadow = false;
  bool has_gbuffer = false;
  bool has_ssao = false;
};

} // namespace vkfw

