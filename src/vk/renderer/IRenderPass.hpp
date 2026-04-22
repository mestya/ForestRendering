#pragma once
#include "vk/scene/common.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/helper.hpp"
#include "vk/core/VkContext.hpp"
namespace vkfw
{

  class VkContext;
  class VkSwapchain;
  struct FrameContext;
  struct RenderTargets;

  class IRenderPass
  {
  public:
    virtual ~IRenderPass() = default;

    virtual bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) = 0;

    virtual void Destroy(VkContext &ctx)
    {
      ctx.Device().waitIdle();
      common_sampler_ = nullptr;
      textures_.clear();
    }

    virtual void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) = 0;
    virtual void Record(FrameContext &frame, RenderTargets &targets) = 0;

    virtual void setDebugParameter(DebugParam &param) {

    };

  protected:
    // 待改，应该所有子类共享一份采样器
    vk::raii::Sampler common_sampler_{nullptr};

    std::vector<TextureResource> textures_;

  protected:
    TextureResource LoadTextureResource(
        vk::raii::Device &device,
        vk::raii::PhysicalDevice &physDevice,
        vk::raii::Queue &queue,
        const std::string &path);
    void LoadTexture(VkContext &ctx, const std::string &path, uint32_t index);
    void CreateCommonSampler(vk::raii::Device &device)
    {
      if (*common_sampler_ != VK_NULL_HANDLE)
        return;
      vk::SamplerCreateInfo sampler_ci{};
      sampler_ci.magFilter = vk::Filter::eLinear;
      sampler_ci.minFilter = vk::Filter::eLinear;
      sampler_ci.addressModeU = vk::SamplerAddressMode::eRepeat;
      sampler_ci.addressModeV = vk::SamplerAddressMode::eRepeat;
      sampler_ci.addressModeW = vk::SamplerAddressMode::eRepeat;
      common_sampler_ = vk::raii::Sampler{device, sampler_ci};
    }
  };

} // namespace vkfw
