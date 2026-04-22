#pragma once

#include <vector>
#include <array>
#include <vulkan/vulkan.hpp>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace vkfw
{
    inline vk::Format FindDepthFormat(vk::raii::PhysicalDevice const &pd)
    {
        std::array<vk::Format, 3> const candidates = {
            vk::Format::eD32Sfloat,
            vk::Format::eD24UnormS8Uint,
            vk::Format::eD32SfloatS8Uint,
        };

        for (auto fmt : candidates)
        {
            auto props = pd.getFormatProperties(fmt);
            if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
                return fmt;
        }
        throw std::runtime_error("No supported depth format");
    }

    inline uint32_t FindMemoryType(vk::raii::PhysicalDevice const &pd, uint32_t type_bits, vk::MemoryPropertyFlags required)
    {
        auto mem_props = pd.getMemoryProperties();
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
        {
            if ((type_bits & (1u << i)) == 0u)
                continue;
            if ((mem_props.memoryTypes[i].propertyFlags & required) == required)
                return i;
        }
        throw std::runtime_error("No suitable memory type");
    }

    inline void TransitionImage(vk::raii::CommandBuffer &cmd,
                                vk::Image image,
                                vk::ImageLayout old_layout,
                                vk::ImageLayout new_layout,
                                vk::ImageAspectFlags aspect,
                                vk::AccessFlags2 src_access,
                                vk::AccessFlags2 dst_access,
                                vk::PipelineStageFlags2 src_stage,
                                vk::PipelineStageFlags2 dst_stage)
    {
        vk::ImageMemoryBarrier2 barrier{};
        barrier.srcStageMask = src_stage;
        barrier.srcAccessMask = src_access;
        barrier.dstStageMask = dst_stage;
        barrier.dstAccessMask = dst_access;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vk::DependencyInfo dep{};
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers = &barrier;
        cmd.pipelineBarrier2(dep);
    }
} // namespace Forest