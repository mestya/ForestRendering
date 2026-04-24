#pragma once

#include "vk/renderer/IRenderPass.hpp"

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace vkfw
{

    class MeshPass final : public IRenderPass
    {
    public:
        explicit MeshPass(std::string model_path);
        bool Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
        void Destroy(VkContext &ctx) override;
        void OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets) override;
        void Record(FrameContext &frame, RenderTargets &targets) override;

    private:
        // 子网格结构体：用于区分树干和树叶
        struct SubMesh
        {
            uint32_t indexCount;
            uint32_t firstIndex;
            uint32_t textureIndex; // 0: 树干, 1: 树叶
        };

        // 森林中树的数量
        uint32_t instance_count_ = 0;

        // Instance Buffer 资源
        vk::raii::Buffer instance_buf_{nullptr};
        vk::raii::DeviceMemory instance_mem_{nullptr};

        // 存储模型矩阵的结构体（对应 Vertex.hpp）
        struct InstanceData
        {
            glm::mat4 model;
        };

        std::string model_path_;
        std::vector<SubMesh> sub_meshes_;

        vk::raii::PipelineLayout pipeline_layout_{nullptr};
        vk::raii::Pipeline pipeline_{nullptr};

        vk::raii::DescriptorSetLayout dsl_{nullptr};
        vk::raii::DescriptorPool dp_{nullptr};

        // 这里存所有的描述符集：每帧 2 个（树干+树叶），如果 3 帧缓冲，这里就有 6 个
        std::vector<vk::raii::DescriptorSet> ds_{};

        std::vector<vk::raii::Buffer> ubo_buf_{};
        std::vector<vk::raii::DeviceMemory> ubo_mem_{};
        std::vector<void *> ubo_map_{};

        vk::raii::Buffer vb_{nullptr};
        vk::raii::DeviceMemory vb_mem_{nullptr};
        vk::raii::Buffer ib_{nullptr};
        vk::raii::DeviceMemory ib_mem_{nullptr};

        uint32_t total_index_count_ = 0;
        glm::mat4 model_matrix_{1.0f};

        // 材质资源数组 待删除
        std::vector<vk::raii::Image> tex_imgs_;
        std::vector<vk::raii::DeviceMemory> tex_mems_;
        std::vector<vk::raii::ImageView> tex_views_;
        vk::raii::Sampler texture_sampler_{nullptr};
    };

} // namespace vkfw
