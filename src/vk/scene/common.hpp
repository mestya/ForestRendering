#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    struct ImGuiResources
    {
        vk::raii::DescriptorPool descriptorPool = nullptr;
    };

    struct alignas(16) CameraUBO
    {
        glm::mat4 view{1.0f};
        glm::mat4 proj{1.0f};
        glm::mat4 model{1.0f};
        glm::mat4 light{1.0f};
        glm::vec4 camera_pos{0.0f};
    };

    struct DebugParam
    {
        bool animation{false};
        bool volumtricl{false};
        bool shadowmap{false};
        float daySpeed = 0.5f;
        float lightX{-52.8f};
        float lightY{50.4f};
        float lightZ{100.0f};
    };

} // namespace Forest