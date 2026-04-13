#pragma once

#include "vk_common.hpp"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace forest::vk {

class Context
{
public:
	void init(GLFWwindow* window);
	void cleanup();

	void draw_frame();
	void request_swapchain_recreate();

	VkDevice device() const { return m_device; }

private:
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphics_family;
		std::optional<uint32_t> present_family;

		bool complete() const { return graphics_family.has_value() && present_family.has_value(); }
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	struct FrameSync
	{
		VkSemaphore image_available = VK_NULL_HANDLE;
		VkSemaphore render_finished = VK_NULL_HANDLE;
		VkFence in_flight = VK_NULL_HANDLE;
	};

	void create_instance();
	void setup_debug_messenger();
	void create_surface();
	void pick_physical_device();
	void create_logical_device();
	void create_command_pool();

	void create_swapchain();
	void create_image_views();
	void create_render_pass();
	void create_framebuffers();
	void create_command_buffers();
	void create_sync_objects();

	void cleanup_swapchain();
	void recreate_swapchain();

	bool check_validation_layer_support() const;
	std::vector<char const*> get_required_instance_extensions() const;

	QueueFamilyIndices find_queue_families(VkPhysicalDevice device) const;
	bool check_device_extension_support(VkPhysicalDevice device) const;
	SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device) const;

	VkSurfaceFormatKHR choose_swap_surface_format(std::vector<VkSurfaceFormatKHR> const& formats) const;
	VkPresentModeKHR choose_swap_present_mode(std::vector<VkPresentModeKHR> const& present_modes) const;
	VkExtent2D choose_swap_extent(VkSurfaceCapabilitiesKHR const& capabilities) const;

	void record_command_buffer(VkCommandBuffer cmd, uint32_t image_index);

private:
	GLFWwindow* m_window = nullptr;

	VkInstance m_instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;

	VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;

	VkQueue m_graphics_queue = VK_NULL_HANDLE;
	VkQueue m_present_queue = VK_NULL_HANDLE;

	VkCommandPool m_command_pool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_command_buffers;

	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchain_images;
	VkFormat m_swapchain_image_format = VK_FORMAT_UNDEFINED;
	VkExtent2D m_swapchain_extent{};
	std::vector<VkImageView> m_swapchain_image_views;

	VkRenderPass m_render_pass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> m_swapchain_framebuffers;

	static constexpr uint32_t k_max_frames_in_flight = 2;
	std::vector<FrameSync> m_frames;
	size_t m_current_frame = 0;

	bool m_framebuffer_resized = false;

	std::vector<char const*> m_enabled_device_extensions;
};

} // namespace forest::vk

