#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{

  // Named attachment registry shared between passes.
  // This is a skeleton; the real version will store Vulkan images/views.
  struct RenderTargets
  {
    struct DepthTarget
    {
      vk::Format format{vk::Format::eUndefined};
      vk::Extent2D extent{};
      vk::Image image{};
      vk::ImageView view{};
      vk::ImageLayout layout{vk::ImageLayout::eUndefined};

      bool Valid() const noexcept { return static_cast<VkImage>(image) != VK_NULL_HANDLE && static_cast<VkImageView>(view) != VK_NULL_HANDLE; }
    };

    DepthTarget shared_depth{};

    bool has_shadow = false;
    bool has_gbuffer = false;
    bool has_ssao = false;
  };

  struct TextureResource
  {
    vk::raii::Image image{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
    vk::raii::ImageView view{nullptr};
  };
} // namespace vkfw
