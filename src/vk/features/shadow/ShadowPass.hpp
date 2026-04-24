#pragma once
#include "vk/renderer/IRenderPass.hpp"
#include <glm/glm.hpp>

#include <functional>

namespace vkfw
{
  class ShadowPass final : public IRenderPass
  {
  public:
    // 阴影通道不需要知道 Swapchain，但需要知道阴影贴图的分辨率
    bool Create(VkContext &ctx, uint32_t shadow_map_res);
    void Destroy(VkContext &ctx) override;
    void Record(FrameContext &frame, RenderTargets &targets) override;

    // 关键：暴露这个函数，供调度者（如 App/Renderer）在 Record 时调用
    // 这样 TerrainPass 就可以把它的 Draw 指令传进来
    void Execute(FrameContext &frame,
                 RenderTargets &targets,
                 const std::function<void(vk::raii::CommandBuffer &, vk::PipelineLayout)> &draw_callback);

    // 供主渲染通道（Terrain）获取阴影贴图的 View 和 Sampler
    vk::raii::ImageView const &GetShadowView() const { return shadow_view_; }
    vk::raii::Sampler const &GetShadowSampler() const { return shadow_sampler_; }

  private:
    void CreateShadowResources(VkContext &ctx, uint32_t res);
    void CreatePipeline(const vk::raii::Device &device, vk::Format depth_format);

    // 阴影资源
    vk::raii::Image shadow_image_{nullptr};
    vk::raii::DeviceMemory shadow_mem_{nullptr};
    vk::raii::ImageView shadow_view_{nullptr};
    vk::raii::Sampler shadow_sampler_{nullptr};

    vk::raii::PipelineLayout pipeline_layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
    uint32_t resolution_;
  };
}