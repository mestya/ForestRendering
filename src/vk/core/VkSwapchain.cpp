#include "vk/core/VkSwapchain.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vk/core/VkContext.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

namespace vkfw {
namespace {

static uint32_t ChooseMinImageCount(vk::SurfaceCapabilitiesKHR const& caps)
{
  uint32_t count = std::max(3u, caps.minImageCount);
  if (caps.maxImageCount > 0 && count > caps.maxImageCount)
    count = caps.maxImageCount;
  return count;
}

static vk::SurfaceFormatKHR ChooseSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& formats)
{
  assert(!formats.empty());
  auto it = std::find_if(formats.begin(), formats.end(), [](auto const& f) {
    return f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
  });
  return (it != formats.end()) ? *it : formats[0];
}

static vk::PresentModeKHR ChoosePresentMode(std::vector<vk::PresentModeKHR> const& modes)
{
  // FIFO is guaranteed.
  auto it = std::find(modes.begin(), modes.end(), vk::PresentModeKHR::eMailbox);
  return (it != modes.end()) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}

static vk::Extent2D ChooseExtent(GLFWwindow* window, vk::SurfaceCapabilitiesKHR const& caps)
{
  if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
    return caps.currentExtent;

  int w = 0, h = 0;
  glfwGetFramebufferSize(window, &w, &h);
  vk::Extent2D e{};
  e.width = std::clamp<uint32_t>(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width);
  e.height = std::clamp<uint32_t>(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
  return e;
}

} // namespace

bool VkSwapchain::Init(VkContext& ctx, SwapchainInfo const& info)
{
  info_ = info;
  CreateSwapchain(ctx);
  CreateImageViews(ctx);
  first_use_.assign(images_.size(), true);
  return true;
}

void VkSwapchain::Recreate(VkContext& ctx)
{
  ctx.Device().waitIdle();
  CleanupSwapchain();
  CreateSwapchain(ctx);
  CreateImageViews(ctx);
  first_use_.assign(images_.size(), true);
}

void VkSwapchain::Shutdown(VkContext& ctx)
{
  ctx.Device().waitIdle();
  CleanupSwapchain();
  info_ = {};
  extent_ = {};
  format_ = vk::Format::eUndefined;
  surface_format_ = {};
  first_use_.clear();
}

void VkSwapchain::CleanupSwapchain()
{
  image_views_.clear();
  images_.clear();
  swapchain_ = nullptr;
}

void VkSwapchain::CreateSwapchain(VkContext& ctx)
{
  auto& pd = ctx.PhysicalDevice();
  auto& device = ctx.Device();

  auto caps = pd.getSurfaceCapabilitiesKHR(*ctx.Surface());
  extent_ = ChooseExtent(ctx.Window(), caps);
  uint32_t min_count = ChooseMinImageCount(caps);

  auto formats = pd.getSurfaceFormatsKHR(*ctx.Surface());
  surface_format_ = ChooseSurfaceFormat(formats);
  format_ = surface_format_.format;

  auto modes = pd.getSurfacePresentModesKHR(*ctx.Surface());
  auto present_mode = ChoosePresentMode(modes);

  vk::SwapchainCreateInfoKHR sci{};
  sci.surface = *ctx.Surface();
  sci.minImageCount = min_count;
  sci.imageFormat = surface_format_.format;
  sci.imageColorSpace = surface_format_.colorSpace;
  sci.imageExtent = extent_;
  sci.imageArrayLayers = 1;
  sci.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  sci.imageSharingMode = vk::SharingMode::eExclusive;
  sci.preTransform = caps.currentTransform;
  sci.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  sci.presentMode = present_mode;
  sci.clipped = VK_TRUE;

  swapchain_ = vk::raii::SwapchainKHR{device, sci};
  images_ = swapchain_.getImages();
}

void VkSwapchain::CreateImageViews(VkContext& ctx)
{
  auto& device = ctx.Device();
  image_views_.clear();
  image_views_.reserve(images_.size());

  vk::ImageViewCreateInfo ivci{};
  ivci.viewType = vk::ImageViewType::e2D;
  ivci.format = surface_format_.format;
  ivci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  ivci.subresourceRange.baseMipLevel = 0;
  ivci.subresourceRange.levelCount = 1;
  ivci.subresourceRange.baseArrayLayer = 0;
  ivci.subresourceRange.layerCount = 1;

  for (auto img : images_) {
    ivci.image = img;
    image_views_.emplace_back(device, ivci);
  }
}

std::pair<vk::Result, uint32_t> VkSwapchain::AcquireNextImage(uint64_t timeout,
                                                              vk::Semaphore image_available,
                                                              vk::Fence fence)
{
  // Vulkan-Hpp may return ResultValue<T>; normalize to (Result, value).
  auto rv = swapchain_.acquireNextImage(timeout, image_available, fence);
  return {rv.result, rv.value};
}

bool VkSwapchain::IsFirstUse(uint32_t image_index) const
{
  return first_use_.at(image_index);
}

void VkSwapchain::MarkUsed(uint32_t image_index)
{
  first_use_.at(image_index) = false;
}

} // namespace vkfw
