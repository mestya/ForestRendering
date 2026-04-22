#include "vk/features/gbuffer/GBufferPass.hpp"

#include "vk/renderer/RenderTargets.hpp"

namespace vkfw
{

  bool GBufferPass::Create(VkContext &, VkSwapchain const &, RenderTargets &targets)
  {
    targets.has_gbuffer = true;
    return true;
  }

  void GBufferPass::Destroy(VkContext &)
  {
    // IRenderPass::Destroy(ctx);
  }

  void GBufferPass::OnSwapchainRecreated(VkContext &, VkSwapchain const &, RenderTargets &) {}

  void GBufferPass::Record(FrameContext &, RenderTargets &) {}

} // namespace vkfw
