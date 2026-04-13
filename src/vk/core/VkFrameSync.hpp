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

class VkFrameSync {
public:
  VkFrameSync() = default;
  ~VkFrameSync() = default;

  bool Init(VkContext& ctx, uint32_t frames_in_flight);
  void Shutdown(VkContext& ctx);

  uint32_t FramesInFlight() const noexcept { return frames_in_flight_; }

  void WaitForFrame(VkContext& ctx, uint32_t frame_index);
  void ResetFence(VkContext& ctx, uint32_t frame_index);

  vk::Semaphore ImageAvailable(uint32_t frame_index) const;
  vk::Fence InFlightFence(uint32_t frame_index) const;
  vk::Semaphore RenderFinished(uint32_t image_index) const;

  void EnsureRenderFinishedSize(VkContext& ctx, uint32_t image_count);

private:
  uint32_t frames_in_flight_ = 0;

  std::vector<vk::raii::Semaphore> image_available_{};
  std::vector<vk::raii::Semaphore> render_finished_{};
  std::vector<vk::raii::Fence> in_flight_{};
};

} // namespace vkfw
