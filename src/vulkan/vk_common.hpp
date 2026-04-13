#pragma once

#include <vulkan/vulkan.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace forest::vk {

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

inline void vk_check(VkResult result, char const* what)
{
	if (result != VK_SUCCESS) {
		throw std::runtime_error(std::string(what) + " (VkResult=" + std::to_string(result) + ")");
	}
}

} // namespace forest::vk

#define VK_CHECK(expr) ::forest::vk::vk_check((expr), #expr)
