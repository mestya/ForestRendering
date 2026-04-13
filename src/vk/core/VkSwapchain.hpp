#pragma once

#include <cstdint>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw {

class VkContext;

struct SwapchainInfo {
  uint32_t width = 0;
  uint32_t height = 0;
};

class VkSwapchain {
public:
  VkSwapchain() = default;
  ~VkSwapchain() = default;

  bool Init(VkContext& ctx, SwapchainInfo const& info);
  void Recreate(VkContext& ctx);
  void Shutdown(VkContext& ctx);

  SwapchainInfo GetInfo() const noexcept { return info_; }
  vk::Extent2D Extent() const noexcept { return extent_; }
  vk::Format Format() const noexcept { return format_; }
  vk::SwapchainKHR Handle() const noexcept { return *swapchain_; }
  vk::SurfaceFormatKHR SurfaceFormat() const noexcept { return surface_format_; }
  uint32_t ImageCount() const noexcept { return static_cast<uint32_t>(images_.size()); }

  vk::Image Image(uint32_t index) const { return images_.at(index); }
  vk::ImageView ImageView(uint32_t index) const { return *image_views_.at(index); }

  std::pair<vk::Result, uint32_t> AcquireNextImage(uint64_t timeout,
                                                   vk::Semaphore image_available,
                                                   vk::Fence fence);

  bool IsFirstUse(uint32_t image_index) const;
  void MarkUsed(uint32_t image_index);

private:
  void CleanupSwapchain();
  void CreateSwapchain(VkContext& ctx);
  void CreateImageViews(VkContext& ctx);

  SwapchainInfo info_{};
  vk::Extent2D extent_{};
  vk::Format format_{vk::Format::eUndefined};

  vk::SurfaceFormatKHR surface_format_{};
  vk::raii::SwapchainKHR swapchain_{nullptr};
  std::vector<vk::Image> images_{};
  std::vector<vk::raii::ImageView> image_views_{};
  std::vector<bool> first_use_{};
};

} // namespace vkfw
