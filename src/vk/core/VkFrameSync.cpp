#include "vk/core/VkFrameSync.hpp"

#include "vk/core/VkContext.hpp"

#include <cassert>
#include <stdexcept>

namespace vkfw {

bool VkFrameSync::Init(VkContext& ctx, uint32_t frames_in_flight)
{
  frames_in_flight_ = frames_in_flight;
  image_available_.clear();
  in_flight_.clear();

  auto& device = ctx.Device();
  image_available_.reserve(frames_in_flight_);
  in_flight_.reserve(frames_in_flight_);

  for (uint32_t i = 0; i < frames_in_flight_; ++i) {
    image_available_.emplace_back(device, vk::SemaphoreCreateInfo{});
    vk::FenceCreateInfo fci{};
    fci.flags = vk::FenceCreateFlagBits::eSignaled;
    in_flight_.emplace_back(device, fci);
  }

  return true;
}

void VkFrameSync::Shutdown(VkContext& ctx)
{
  ctx.Device().waitIdle();
  image_available_.clear();
  render_finished_.clear();
  in_flight_.clear();
  frames_in_flight_ = 0;
}

void VkFrameSync::EnsureRenderFinishedSize(VkContext& ctx, uint32_t image_count)
{
  auto& device = ctx.Device();
  if (render_finished_.size() == image_count)
    return;

  render_finished_.clear();
  render_finished_.reserve(image_count);
  for (uint32_t i = 0; i < image_count; ++i) {
    render_finished_.emplace_back(device, vk::SemaphoreCreateInfo{});
  }
}

void VkFrameSync::WaitForFrame(VkContext& ctx, uint32_t frame_index)
{
  auto& device = ctx.Device();
  auto res = device.waitForFences(*in_flight_.at(frame_index), vk::True, UINT64_MAX);
  if (res != vk::Result::eSuccess)
    throw std::runtime_error("waitForFences failed");
}

void VkFrameSync::ResetFence(VkContext& ctx, uint32_t frame_index)
{
  ctx.Device().resetFences(*in_flight_.at(frame_index));
}

vk::Semaphore VkFrameSync::ImageAvailable(uint32_t frame_index) const
{
  return *image_available_.at(frame_index);
}

vk::Fence VkFrameSync::InFlightFence(uint32_t frame_index) const
{
  return *in_flight_.at(frame_index);
}

vk::Semaphore VkFrameSync::RenderFinished(uint32_t image_index) const
{
  return *render_finished_.at(image_index);
}

} // namespace vkfw
