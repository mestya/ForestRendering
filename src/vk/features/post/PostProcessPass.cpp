#include "vk/features/post/PostProcessPass.hpp"

#include "vk/renderer/RenderTargets.hpp"

namespace vkfw {

bool PostProcessPass::Create(VkContext&, VkSwapchain const&, RenderTargets& targets)
{
  targets.has_ssao = true; // placeholder
  return true;
}

void PostProcessPass::Destroy(VkContext&) {}

void PostProcessPass::OnSwapchainRecreated(VkContext&, VkSwapchain const&, RenderTargets&) {}

void PostProcessPass::Record(FrameContext&, RenderTargets&) {}

} // namespace vkfw
