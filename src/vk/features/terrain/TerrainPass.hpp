#pragma once
#include "vk/scene/Model.hpp"
#include "vk/renderer/IRenderPass.hpp"

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{
  class TerrainPass final : public IRenderPass
  {
  public:
    bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Destroy(VkContext &ctx) override;
    void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
    void Record(FrameContext &frame, RenderTargets &targets) override;

  private:
    void updateVertexBuffer();

  private:
    struct alignas(16) CameraUBO
    {
      glm::mat4 view{1.0f};
      glm::mat4 proj{1.0f};
      glm::mat4 model{1.0f};
      glm::vec4 camera_pos{0.0f};
    };
    vk::raii::PipelineLayout pipeline_layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
    vk::raii::Buffer vertex_buffer_{nullptr};
    vk::raii::DeviceMemory vertex_memory_{nullptr};
    vk::raii::Buffer index_buffer_{nullptr};
    vk::raii::DeviceMemory index_memory_{nullptr};
    Model terrtain_;

    vk::raii::DescriptorSetLayout dsl_{nullptr};
    vk::raii::DescriptorPool dp_{nullptr};
    std::vector<vk::raii::DescriptorSet> ds_{};

    std::vector<vk::raii::Buffer> ubo_buf_{};
    std::vector<vk::raii::DeviceMemory> ubo_mem_{};
    std::vector<void *> ubo_map_{};
  };

} // namespace vkfw
