#include "vk/features/geometry/GeometryPass.hpp"

#include "vk/renderer/RenderTargets.hpp"

namespace vkfw
{

  bool GeometryPass::Create(VkContext &, VkSwapchain const &, RenderTargets &targets)
  {
    targets.has_gbuffer = true; // placeholder: geometry usually feeds gbuffer/depth
    return true;
  }

  void GeometryPass::Destroy(VkContext &)
  {
    // IRenderPass::Destroy(ctx);
  }

  void GeometryPass::OnSwapchainRecreated(VkContext &, VkSwapchain const &, RenderTargets &) {}

  void GeometryPass::Record(FrameContext &, RenderTargets &) {}

} // namespace vkfw
