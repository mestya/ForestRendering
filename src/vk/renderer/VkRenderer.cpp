#include "vk/renderer/VkRenderer.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkFrameSync.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/IRenderPass.hpp"

#include <cassert>
#include <stdexcept>

namespace vkfw {

void VkRenderer::AddPass(std::unique_ptr<IRenderPass> pass)
{
  passes_.push_back(std::move(pass));
}

void VkRenderer::CreateCommandResources(VkContext& ctx, VkFrameSync& sync)
{
  auto& device = ctx.Device();

  vk::CommandPoolCreateInfo cpci{};
  cpci.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
  cpci.queueFamilyIndex = ctx.GraphicsQueueFamilyIndex();
  command_pool_ = vk::raii::CommandPool{device, cpci};

  vk::CommandBufferAllocateInfo cbai{};
  cbai.commandPool = *command_pool_;
  cbai.level = vk::CommandBufferLevel::ePrimary;
  cbai.commandBufferCount = sync.FramesInFlight();
  command_buffers_ = device.allocateCommandBuffers(cbai);
}

void VkRenderer::DestroyCommandResources(VkContext& ctx)
{
  ctx.Device().waitIdle();
  command_buffers_.clear();
  command_pool_ = nullptr;
}

bool VkRenderer::Create(VkContext& ctx, VkSwapchain& swapchain, VkFrameSync& sync)
{
  sync.EnsureRenderFinishedSize(ctx, swapchain.ImageCount());
  CreateCommandResources(ctx, sync);

  for (auto& pass : passes_) {
    if (!pass->Create(ctx, swapchain, targets_))
      return false;
  }
  return true;
}

void VkRenderer::Destroy(VkContext& ctx)
{
  for (auto& pass : passes_) {
    pass->Destroy(ctx);
  }
  DestroyCommandResources(ctx);
}

void VkRenderer::OnSwapchainRecreated(VkContext& ctx, VkSwapchain& swapchain, VkFrameSync& sync)
{
  sync.EnsureRenderFinishedSize(ctx, swapchain.ImageCount());
  for (auto& pass : passes_) {
    pass->OnSwapchainRecreated(ctx, swapchain, targets_);
  }
}

bool VkRenderer::DrawFrame(VkContext& ctx, VkSwapchain& swapchain, VkFrameSync& sync)
{
  sync.WaitForFrame(ctx, frame_index_);

  auto [acq_result, image_index] =
      swapchain.AcquireNextImage(UINT64_MAX, sync.ImageAvailable(frame_index_), vk::Fence{});

  if (acq_result == vk::Result::eErrorOutOfDateKHR)
    return false;
  if (acq_result != vk::Result::eSuccess && acq_result != vk::Result::eSuboptimalKHR)
    throw std::runtime_error("acquireNextImage failed");

  sync.ResetFence(ctx, frame_index_);

  auto& cmd = command_buffers_.at(frame_index_);
  cmd.reset();
  cmd.begin(vk::CommandBufferBeginInfo{});

  FrameContext frame{};
  frame.cmd = &cmd;
  frame.frame_index = frame_index_;
  frame.image_index = image_index;
  frame.swapchain_extent = swapchain.Extent();
  frame.swapchain_format = swapchain.Format();
  frame.swapchain_image = swapchain.Image(image_index);
  frame.swapchain_image_view = swapchain.ImageView(image_index);
  frame.swapchain_old_layout =
      swapchain.IsFirstUse(image_index) ? vk::ImageLayout::eUndefined : vk::ImageLayout::ePresentSrcKHR;

  for (auto& pass : passes_) {
    pass->Record(frame, targets_);
  }

  cmd.end();
  swapchain.MarkUsed(image_index);

  vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  vk::SubmitInfo submit{};
  submit.waitSemaphoreCount = 1;
  auto image_avail = sync.ImageAvailable(frame_index_);
  submit.pWaitSemaphores = &image_avail;
  submit.pWaitDstStageMask = &wait_stage;
  submit.commandBufferCount = 1;
  auto raw_cmd = *cmd;
  submit.pCommandBuffers = &raw_cmd;
  submit.signalSemaphoreCount = 1;
  auto render_finished = sync.RenderFinished(image_index);
  submit.pSignalSemaphores = &render_finished;

  ctx.GraphicsQueue().submit(submit, sync.InFlightFence(frame_index_));

  vk::PresentInfoKHR present{};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &render_finished;
  present.swapchainCount = 1;
  auto sc = swapchain.Handle();
  present.pSwapchains = &sc;
  present.pImageIndices = &image_index;

  auto pres_result = ctx.GraphicsQueue().presentKHR(present);
  if (pres_result == vk::Result::eErrorOutOfDateKHR || pres_result == vk::Result::eSuboptimalKHR)
    return false;
  if (pres_result != vk::Result::eSuccess)
    throw std::runtime_error("presentKHR failed");

  frame_index_ = (frame_index_ + 1) % sync.FramesInFlight();
  return true;
}

} // namespace vkfw

