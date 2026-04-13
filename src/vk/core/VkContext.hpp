#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

struct GLFWwindow;

namespace vkfw {

struct ContextCreateInfo {
  // Non-owning.
  ::GLFWwindow* window = nullptr;
  bool enable_validation = true;
};

class VkContext {
public:
  VkContext();
  ~VkContext();

  VkContext(VkContext const&) = delete;
  VkContext& operator=(VkContext const&) = delete;

  VkContext(VkContext&&) noexcept;
  VkContext& operator=(VkContext&&) noexcept;

  bool Init(ContextCreateInfo const& info);
  void Shutdown();

  bool IsInitialized() const noexcept;

  vk::raii::Context& Context() const;
  vk::raii::Instance& Instance() const;
  vk::raii::SurfaceKHR& Surface() const;
  ::GLFWwindow* Window() const noexcept;
  vk::raii::PhysicalDevice& PhysicalDevice() const;
  vk::raii::Device& Device() const;
  vk::raii::Queue& GraphicsQueue() const;
  uint32_t GraphicsQueueFamilyIndex() const noexcept;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace vkfw
