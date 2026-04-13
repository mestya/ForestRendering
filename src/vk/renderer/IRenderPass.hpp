#pragma once

namespace vkfw {

class VkContext;
class VkSwapchain;
struct FrameContext;
struct RenderTargets;

class IRenderPass {
public:
  virtual ~IRenderPass() = default;

  virtual bool Create(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) = 0;
  virtual void Destroy(VkContext& ctx) = 0;
  virtual void OnSwapchainRecreated(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) = 0;
  virtual void Record(FrameContext& frame, RenderTargets& targets) = 0;
};

} // namespace vkfw

