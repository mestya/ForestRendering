#pragma once

#include <cstdint>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw {

struct FrameContext {
  vk::raii::CommandBuffer* cmd = nullptr; // non-owning
  uint32_t frame_index = 0;
  uint32_t image_index = 0;
  vk::Extent2D swapchain_extent{};
  vk::Format swapchain_format{vk::Format::eUndefined};
  vk::Image swapchain_image{};
  vk::ImageView swapchain_image_view{};
  vk::ImageLayout swapchain_old_layout{vk::ImageLayout::ePresentSrcKHR};
};

} // namespace vkfw
