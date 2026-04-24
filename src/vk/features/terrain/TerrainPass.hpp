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
    vk::raii::DescriptorSetLayout CreateDescriptorSetLayout(const vk::raii::Device &device);
    vk::raii::DescriptorPool CreateDescriptorPool(const vk::raii::Device &device, const int image_count);
    /**
     * @brief 为每一帧分配 Uniform Buffer 及其背后的内存映射
     * @param device 逻辑设备句柄
     * @param mem_props 物理设备内存属性（用于查找内存类型）
     * @param image_count 需要创建的数量（通常等于 Swapchain Image Count）
     */
    void CreateUniformBuffers(
        const VkContext &ctx,
        const vk::PhysicalDeviceMemoryProperties &mem_props,
        uint32_t image_count);
    void CreateVertexBuffer(
        const VkContext &ctx,
        const vk::PhysicalDeviceMemoryProperties &mem_props,
        const Model &model);
    void CreateIndexBuffer(
        const VkContext &ctx,
        const vk::PhysicalDeviceMemoryProperties &mem_props,
        const Model &model);
    void CreatePipeline(const vk::raii::Device &device,
                        const std::string &shader_path,
                        vk::Format color_format,
                        vk::Format depth_format);
    void UpdateDescriptorSets(const vk::raii::Device &device, uint32_t image_count);

  private:
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
