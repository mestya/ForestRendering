#pragma once

#include <memory>
#include <vector>

#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"

namespace vkfw {

class VkContext;
class VkSwapchain;
class VkFrameSync;
class IRenderPass;

class VkRenderer {
public:
  VkRenderer() = default;
  ~VkRenderer() = default;

  VkRenderer(VkRenderer const&) = delete;
  VkRenderer& operator=(VkRenderer const&) = delete;

  void AddPass(std::unique_ptr<IRenderPass> pass);

  bool Create(VkContext& ctx, VkSwapchain& swapchain, VkFrameSync& sync);
  void Destroy(VkContext& ctx);
  void OnSwapchainRecreated(VkContext& ctx, VkSwapchain& swapchain, VkFrameSync& sync);

  // Returns false when swapchain needs recreation.
  bool DrawFrame(VkContext& ctx, VkSwapchain& swapchain, VkFrameSync& sync);

  RenderTargets& Targets() noexcept { return targets_; }
  RenderTargets const& Targets() const noexcept { return targets_; }

private:
  void CreateCommandResources(VkContext& ctx, VkFrameSync& sync);
  void DestroyCommandResources(VkContext& ctx);

  std::vector<std::unique_ptr<IRenderPass>> passes_;
  RenderTargets targets_{};

  vk::raii::CommandPool command_pool_{nullptr};
  std::vector<vk::raii::CommandBuffer> command_buffers_{};
  uint32_t frame_index_ = 0;
};

} // namespace vkfw
