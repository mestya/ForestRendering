#include "vk/renderer/VkRenderer.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkFrameSync.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/IRenderPass.hpp"
#include "vk/renderer/helper.hpp"

#include <array>
#include <cassert>
#include <stdexcept>

namespace vkfw
{

  // namespace
  // {

  //   vk::Format FindDepthFormat(vk::raii::PhysicalDevice const &pd)
  //   {
  //     std::array<vk::Format, 3> const candidates = {
  //         vk::Format::eD32Sfloat,
  //         vk::Format::eD24UnormS8Uint,
  //         vk::Format::eD32SfloatS8Uint,
  //     };

  //     for (auto fmt : candidates)
  //     {
  //       auto props = pd.getFormatProperties(fmt);
  //       if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
  //         return fmt;
  //     }
  //     throw std::runtime_error("No supported depth format");
  //   }

  //   uint32_t FindMemoryType(vk::raii::PhysicalDevice const &pd, uint32_t type_bits, vk::MemoryPropertyFlags required)
  //   {
  //     auto mem_props = pd.getMemoryProperties();
  //     for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
  //     {
  //       if ((type_bits & (1u << i)) == 0u)
  //         continue;
  //       if ((mem_props.memoryTypes[i].propertyFlags & required) == required)
  //         return i;
  //     }
  //     throw std::runtime_error("No suitable memory type");
  //   }

  //   void TransitionImage(vk::raii::CommandBuffer &cmd,
  //                        vk::Image image,
  //                        vk::ImageLayout old_layout,
  //                        vk::ImageLayout new_layout,
  //                        vk::ImageAspectFlags aspect,
  //                        vk::AccessFlags2 src_access,
  //                        vk::AccessFlags2 dst_access,
  //                        vk::PipelineStageFlags2 src_stage,
  //                        vk::PipelineStageFlags2 dst_stage)
  //   {
  //     vk::ImageMemoryBarrier2 barrier{};
  //     barrier.srcStageMask = src_stage;
  //     barrier.srcAccessMask = src_access;
  //     barrier.dstStageMask = dst_stage;
  //     barrier.dstAccessMask = dst_access;
  //     barrier.oldLayout = old_layout;
  //     barrier.newLayout = new_layout;
  //     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  //     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  //     barrier.image = image;
  //     barrier.subresourceRange.aspectMask = aspect;
  //     barrier.subresourceRange.baseMipLevel = 0;
  //     barrier.subresourceRange.levelCount = 1;
  //     barrier.subresourceRange.baseArrayLayer = 0;
  //     barrier.subresourceRange.layerCount = 1;

  //     vk::DependencyInfo dep{};
  //     dep.imageMemoryBarrierCount = 1;
  //     dep.pImageMemoryBarriers = &barrier;
  //     cmd.pipelineBarrier2(dep);
  //   }

  // } // namespace

  void VkRenderer::AddPass(std::unique_ptr<IRenderPass> pass)
  {
    passes_.push_back(std::move(pass));
  }

  void VkRenderer::SyncSharedDepthTargets() noexcept
  {
    targets_.shared_depth.format = shared_depth_format_;
    targets_.shared_depth.extent = shared_depth_extent_;
    targets_.shared_depth.layout = shared_depth_layout_;
    targets_.shared_depth.image = static_cast<vk::Image>(shared_depth_img_);
    targets_.shared_depth.view = static_cast<vk::ImageView>(shared_depth_view_);
  }

  void VkRenderer::CreateCommandResources(VkContext &ctx, VkFrameSync &sync)
  {
    auto &device = ctx.Device();

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

  void VkRenderer::CreateSharedDepth(VkContext &ctx, vk::Extent2D extent)
  {
    DestroySharedDepth(ctx);

    shared_depth_extent_ = extent;
    shared_depth_format_ = FindDepthFormat(ctx.PhysicalDevice());
    shared_depth_layout_ = vk::ImageLayout::eUndefined;

    auto &device = ctx.Device();

    vk::ImageCreateInfo ici{};
    ici.imageType = vk::ImageType::e2D;
    ici.format = shared_depth_format_;
    ici.extent = vk::Extent3D{extent.width, extent.height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = vk::SampleCountFlagBits::e1;
    ici.tiling = vk::ImageTiling::eOptimal;
    ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    ici.sharingMode = vk::SharingMode::eExclusive;
    ici.initialLayout = vk::ImageLayout::eUndefined;

    shared_depth_img_ = vk::raii::Image{device, ici};

    auto req = shared_depth_img_.getMemoryRequirements();
    vk::MemoryAllocateInfo mai{};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    shared_depth_mem_ = vk::raii::DeviceMemory{device, mai};
    shared_depth_img_.bindMemory(*shared_depth_mem_, 0);

    vk::ImageViewCreateInfo ivci{};
    ivci.image = *shared_depth_img_;
    ivci.viewType = vk::ImageViewType::e2D;
    ivci.format = shared_depth_format_;
    ivci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    shared_depth_view_ = vk::raii::ImageView{device, ivci};

    SyncSharedDepthTargets();
  }

  void VkRenderer::DestroySharedDepth(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    shared_depth_view_ = nullptr;
    shared_depth_img_ = nullptr;
    shared_depth_mem_ = nullptr;
    shared_depth_extent_ = {};
    shared_depth_format_ = vk::Format::eUndefined;
    shared_depth_layout_ = vk::ImageLayout::eUndefined;
    SyncSharedDepthTargets();
  }

  void VkRenderer::DestroyCommandResources(VkContext &ctx)
  {
    ctx.Device().waitIdle();
    command_buffers_.clear();
    command_pool_ = nullptr;
  }

  bool VkRenderer::Create(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync, DebugParam &param)
  {
    sync.EnsureRenderFinishedSize(ctx, swapchain.ImageCount());
    CreateCommandResources(ctx, sync);
    CreateSharedDepth(ctx, swapchain.Extent());

    for (auto &pass : passes_)
    {
      if (!pass->Create(ctx, swapchain, targets_))
        return false;
      pass->setDebugParameter(param);
    }
    return true;
  }

  void VkRenderer::Destroy(VkContext &ctx)
  {
    for (auto &pass : passes_)
    {
      pass->Destroy(ctx);
    }
    DestroySharedDepth(ctx);
    DestroyCommandResources(ctx);
  }

  void VkRenderer::OnSwapchainRecreated(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync)
  {
    sync.EnsureRenderFinishedSize(ctx, swapchain.ImageCount());
    CreateSharedDepth(ctx, swapchain.Extent());
    for (auto &pass : passes_)
    {
      pass->OnSwapchainRecreated(ctx, swapchain, targets_);
    }
  }

  bool VkRenderer::DrawFrame(VkContext &ctx, VkSwapchain &swapchain, VkFrameSync &sync, FrameGlobals const &globals)
  {
    sync.WaitForFrame(ctx, frame_index_);

    auto [acq_result, image_index] =
        swapchain.AcquireNextImage(UINT64_MAX, sync.ImageAvailable(frame_index_), vk::Fence{});

    if (acq_result == vk::Result::eErrorOutOfDateKHR)
      return false;
    if (acq_result != vk::Result::eSuccess && acq_result != vk::Result::eSuboptimalKHR)
      throw std::runtime_error("acquireNextImage failed");

    sync.ResetFence(ctx, frame_index_);

    auto &cmd = command_buffers_.at(frame_index_);
    cmd.reset();
    cmd.begin(vk::CommandBufferBeginInfo{});

    // Transition swapchain image to a renderable layout once per frame.
    vk::ImageLayout const initial_layout =
        swapchain.IsFirstUse(image_index) ? vk::ImageLayout::eUndefined : vk::ImageLayout::ePresentSrcKHR;
    TransitionImage(cmd,
                    swapchain.Image(image_index),
                    initial_layout,
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageAspectFlagBits::eColor,
                    {},
                    vk::AccessFlagBits2::eColorAttachmentWrite,
                    vk::PipelineStageFlagBits2::eTopOfPipe,
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::Image const raw_depth_img = static_cast<vk::Image>(shared_depth_img_);
    if (static_cast<VkImage>(raw_depth_img) != VK_NULL_HANDLE && shared_depth_layout_ != vk::ImageLayout::eDepthAttachmentOptimal)
    {
      TransitionImage(cmd,
                      raw_depth_img,
                      shared_depth_layout_,
                      vk::ImageLayout::eDepthAttachmentOptimal,
                      vk::ImageAspectFlagBits::eDepth,
                      {},
                      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                      vk::PipelineStageFlagBits2::eTopOfPipe,
                      vk::PipelineStageFlagBits2::eEarlyFragmentTests);
      shared_depth_layout_ = vk::ImageLayout::eDepthAttachmentOptimal;
      SyncSharedDepthTargets();
    }

    FrameContext frame{};
    frame.cmd = &cmd;
    frame.frame_index = frame_index_;
    frame.image_index = image_index;
    frame.swapchain_extent = swapchain.Extent();
    frame.swapchain_format = swapchain.Format();
    frame.swapchain_image = swapchain.Image(image_index);
    frame.swapchain_image_view = swapchain.ImageView(image_index);
    frame.swapchain_old_layout = vk::ImageLayout::eColorAttachmentOptimal;
    frame.globals = &globals;
    for (auto &pass : passes_)
    {
      pass->Record(frame, targets_);
    }

    // If the final pass didn't transition to Present, do it here.
    if (frame.swapchain_old_layout != vk::ImageLayout::ePresentSrcKHR)
    {
      TransitionImage(cmd,
                      frame.swapchain_image,
                      frame.swapchain_old_layout,
                      vk::ImageLayout::ePresentSrcKHR,
                      vk::ImageAspectFlagBits::eColor,
                      vk::AccessFlagBits2::eColorAttachmentWrite,
                      {},
                      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                      vk::PipelineStageFlagBits2::eBottomOfPipe);
      frame.swapchain_old_layout = vk::ImageLayout::ePresentSrcKHR;
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
