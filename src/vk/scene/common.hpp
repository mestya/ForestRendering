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

namespace vkfw {

    struct ImGuiResources {
        vk::raii::DescriptorPool descriptorPool = nullptr;
    };

    struct DebugParam{
        bool animation{false};
        bool volumtricl{false};
        bool shadowmap{false};
    };


} // namespace Forest