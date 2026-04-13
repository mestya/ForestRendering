#include "vk/features/shadow/ShadowPass.hpp"

#include "vk/renderer/RenderTargets.hpp"

namespace vkfw {

bool ShadowPass::Create(VkContext&, VkSwapchain const&, RenderTargets& targets)
{
  targets.has_shadow = true;
  return true;
}

void ShadowPass::Destroy(VkContext&) {}

void ShadowPass::OnSwapchainRecreated(VkContext&, VkSwapchain const&, RenderTargets&) {}

void ShadowPass::Record(FrameContext&, RenderTargets&) {}

} // namespace vkfw
