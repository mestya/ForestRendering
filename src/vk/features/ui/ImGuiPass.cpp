#include "vk/features/ui/ImGuiPass.hpp"

namespace vkfw {

bool ImGuiPass::Create(VkContext&, VkSwapchain const&, RenderTargets&)
{
  return true;
}

void ImGuiPass::Destroy(VkContext&) {}

void ImGuiPass::OnSwapchainRecreated(VkContext&, VkSwapchain const&, RenderTargets&) {}

void ImGuiPass::Record(FrameContext&, RenderTargets&) {}

} // namespace vkfw

