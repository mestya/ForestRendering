#pragma once

#include <memory>

class VkMainApp {
public:
  VkMainApp();
  ~VkMainApp();

  VkMainApp(VkMainApp const&) = delete;
  VkMainApp& operator=(VkMainApp const&) = delete;

  VkMainApp(VkMainApp&&) noexcept;
  VkMainApp& operator=(VkMainApp&&) noexcept;

  // Run Vulkan triangle demo. Returns 0 on success.
  int Run();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Convenience entry for main().
int RunVkTriangleApp();
