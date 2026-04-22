#pragma once
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#include "vk/renderer/IRenderPass.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace vkfw {


class ImGuiPass final : public IRenderPass {
public:
  bool Create(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) override;
  void Destroy(VkContext& ctx) override;
  void OnSwapchainRecreated(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) override;
  void Record(FrameContext& frame, RenderTargets& targets) override;
  void setDebugParameter(DebugParam& param) override;
private:
    void InitImGuiDynamic(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets);
private:
    vkfw::ImGuiResources imu_resources_;
    DebugParam* debugParameter_ = nullptr;
    VkFormat imgui_color_format_ = VK_FORMAT_UNDEFINED;
};

} // namespace vkfw
