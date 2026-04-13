#pragma once

#include "vk/renderer/IRenderPass.hpp"

namespace vkfw {

class ImGuiPass final : public IRenderPass {
public:
  bool Create(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) override;
  void Destroy(VkContext& ctx) override;
  void OnSwapchainRecreated(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) override;
  void Record(FrameContext& frame, RenderTargets& targets) override;
};

} // namespace vkfw

