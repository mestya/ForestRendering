#include "Context.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace forest::vk {
namespace {

constexpr char const* k_validation_layer = "VK_LAYER_KHRONOS_validation";

VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
    void*)
{
	(void)type;
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cerr << "[Vulkan] " << callback_data->pMessage << "\n";
	}
	return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerCreateInfoEXT const* create_info,
    VkAllocationCallbacks const* allocator,
    VkDebugUtilsMessengerEXT* messenger)
{
	auto const fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
	    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	if (fn == nullptr) return VK_ERROR_EXTENSION_NOT_PRESENT;
	return fn(instance, create_info, allocator, messenger);
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks const* allocator)
{
	auto const fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
	    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (fn != nullptr) fn(instance, messenger, allocator);
}

} // namespace

void Context::init(GLFWwindow* window)
{
	m_window = window;

	create_instance();
	setup_debug_messenger();
	create_surface();
	pick_physical_device();
	create_logical_device();
	create_command_pool();

	create_swapchain();
	create_image_views();
	create_render_pass();
	create_framebuffers();
	create_command_buffers();
	create_sync_objects();
}

void Context::cleanup()
{
	if (m_device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(m_device);
	}

	for (auto& frame : m_frames) {
		if (frame.image_available) vkDestroySemaphore(m_device, frame.image_available, nullptr);
		if (frame.render_finished) vkDestroySemaphore(m_device, frame.render_finished, nullptr);
		if (frame.in_flight) vkDestroyFence(m_device, frame.in_flight, nullptr);
	}
	m_frames.clear();

	cleanup_swapchain();

	if (m_command_pool) vkDestroyCommandPool(m_device, m_command_pool, nullptr);

	if (m_device) vkDestroyDevice(m_device, nullptr);
	if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

	if (m_debug_messenger) DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
	if (m_instance) vkDestroyInstance(m_instance, nullptr);

	m_window = nullptr;
}

void Context::request_swapchain_recreate()
{
	m_framebuffer_resized = true;
}

bool Context::check_validation_layer_support() const
{
	uint32_t layer_count = 0;
	VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));
	std::vector<VkLayerProperties> layers(layer_count);
	VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_count, layers.data()));

	for (auto const& layer : layers) {
		if (std::strcmp(layer.layerName, k_validation_layer) == 0) return true;
	}
	return false;
}

std::vector<char const*> Context::get_required_instance_extensions() const
{
	uint32_t glfw_extension_count = 0;
	char const** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	if (glfw_extensions == nullptr || glfw_extension_count == 0) {
		throw std::runtime_error("glfwGetRequiredInstanceExtensions() returned no extensions");
	}

	std::vector<char const*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

#if defined(__APPLE__)
	extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

#if !defined(NDEBUG)
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	return extensions;
}

void Context::create_instance()
{
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "ForestSceneVK";
	app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.pEngineName = "ForestVK";
	app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.apiVersion = VK_API_VERSION_1_2;

	auto extensions = get_required_instance_extensions();

#if !defined(NDEBUG)
	bool const enable_validation = check_validation_layer_support();
	std::vector<char const*> layers;
	if (enable_validation) {
		layers.push_back(k_validation_layer);
	} else {
		std::cerr << "[Vulkan] Validation layer not found; continuing without it.\n";
	}
#else
	std::vector<char const*> layers;
#endif

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	create_info.ppEnabledExtensionNames = extensions.data();
	create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
	create_info.ppEnabledLayerNames = layers.data();

#if defined(__APPLE__)
	create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

	VK_CHECK(vkCreateInstance(&create_info, nullptr, &m_instance));
}

void Context::setup_debug_messenger()
{
#if defined(NDEBUG)
	return;
#else
	VkDebugUtilsMessengerCreateInfoEXT create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity =
	    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
	    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType =
	    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
	    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debug_callback;

	if (CreateDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger) != VK_SUCCESS) {
		std::cerr << "[Vulkan] Failed to set up debug messenger.\n";
	}
#endif
}

void Context::create_surface()
{
	VK_CHECK(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface));
}

Context::QueueFamilyIndices Context::find_queue_families(VkPhysicalDevice device) const
{
	QueueFamilyIndices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

	for (uint32_t i = 0; i < queue_family_count; ++i) {
		auto const& q = queue_families[i];

		if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphics_family = i;

		VkBool32 present_support = VK_FALSE;
		VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support));
		if (present_support) indices.present_family = i;

		if (indices.complete()) break;
	}

	return indices;
}

bool Context::check_device_extension_support(VkPhysicalDevice device) const
{
	uint32_t extension_count = 0;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr));
	std::vector<VkExtensionProperties> available(extension_count);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available.data()));

	std::set<std::string> required(m_enabled_device_extensions.begin(), m_enabled_device_extensions.end());
	for (auto const& ext : available) required.erase(ext.extensionName);

	return required.empty();
}

Context::SwapchainSupportDetails Context::query_swapchain_support(VkPhysicalDevice device) const
{
	SwapchainSupportDetails details;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities));

	uint32_t format_count = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr));
	if (format_count != 0) {
		details.formats.resize(format_count);
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data()));
	}

	uint32_t present_mode_count = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr));
	if (present_mode_count != 0) {
		details.present_modes.resize(present_mode_count);
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, details.present_modes.data()));
	}

	return details;
}

void Context::pick_physical_device()
{
	uint32_t device_count = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr));
	if (device_count == 0) throw std::runtime_error("No Vulkan-compatible GPUs found.");

	std::vector<VkPhysicalDevice> devices(device_count);
	VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data()));

	// Base required extensions.
	m_enabled_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	for (auto const& device : devices) {
		auto const indices = find_queue_families(device);
		if (!indices.complete()) continue;

		// MoltenVK portability subset is optional, but must be enabled if present.
		uint32_t extension_count = 0;
		VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr));
		std::vector<VkExtensionProperties> available(extension_count);
		VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available.data()));

		bool has_portability_subset = false;
		for (auto const& ext : available) {
			if (std::strcmp(ext.extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0) {
				has_portability_subset = true;
				break;
			}
		}

		if (has_portability_subset) {
			bool already_enabled = false;
			for (auto const* ext : m_enabled_device_extensions) {
				if (std::strcmp(ext, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0) {
					already_enabled = true;
					break;
				}
			}
			if (!already_enabled) m_enabled_device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
		}

		if (!check_device_extension_support(device)) continue;

		auto const swapchain_support = query_swapchain_support(device);
		if (swapchain_support.formats.empty() || swapchain_support.present_modes.empty()) continue;

		m_physical_device = device;
		return;
	}

	throw std::runtime_error("Failed to find a suitable Vulkan GPU.");
}

void Context::create_logical_device()
{
	auto const indices = find_queue_families(m_physical_device);

	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::set<uint32_t> unique_queue_families = {indices.graphics_family.value(), indices.present_family.value()};

	float const queue_priority = 1.0f;
	for (uint32_t queue_family : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	VkPhysicalDeviceFeatures device_features{};

	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.pEnabledFeatures = &device_features;
	create_info.enabledExtensionCount = static_cast<uint32_t>(m_enabled_device_extensions.size());
	create_info.ppEnabledExtensionNames = m_enabled_device_extensions.data();

#if !defined(NDEBUG)
	std::vector<char const*> layers;
	if (check_validation_layer_support()) layers.push_back(k_validation_layer);
	create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
	create_info.ppEnabledLayerNames = layers.data();
#endif

	VK_CHECK(vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device));

	vkGetDeviceQueue(m_device, indices.graphics_family.value(), 0, &m_graphics_queue);
	vkGetDeviceQueue(m_device, indices.present_family.value(), 0, &m_present_queue);
}

void Context::create_command_pool()
{
	auto const indices = find_queue_families(m_physical_device);

	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = indices.graphics_family.value();

	VK_CHECK(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool));
}

VkSurfaceFormatKHR Context::choose_swap_surface_format(std::vector<VkSurfaceFormatKHR> const& formats) const
{
	for (auto const& fmt : formats) {
		if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return fmt;
		}
	}
	return formats.front();
}

VkPresentModeKHR Context::choose_swap_present_mode(std::vector<VkPresentModeKHR> const& present_modes) const
{
	for (auto const& mode : present_modes) {
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Context::choose_swap_extent(VkSurfaceCapabilitiesKHR const& capabilities) const
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}

	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);

	VkExtent2D actual_extent{
	    static_cast<uint32_t>(width),
	    static_cast<uint32_t>(height),
	};

	actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	return actual_extent;
}

void Context::create_swapchain()
{
	auto const support = query_swapchain_support(m_physical_device);
	auto const surface_format = choose_swap_surface_format(support.formats);
	auto const present_mode = choose_swap_present_mode(support.present_modes);
	auto const extent = choose_swap_extent(support.capabilities);

	uint32_t image_count = support.capabilities.minImageCount + 1;
	if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount) {
		image_count = support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = m_surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto const indices = find_queue_families(m_physical_device);
	uint32_t queue_family_indices[] = {indices.graphics_family.value(), indices.present_family.value()};
	if (indices.graphics_family != indices.present_family) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	create_info.preTransform = support.capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK(vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain));

	uint32_t swapchain_image_count = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_image_count, nullptr));
	m_swapchain_images.resize(swapchain_image_count);
	VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_image_count, m_swapchain_images.data()));

	m_swapchain_image_format = surface_format.format;
	m_swapchain_extent = extent;
}

void Context::create_image_views()
{
	m_swapchain_image_views.resize(m_swapchain_images.size());

	for (size_t i = 0; i < m_swapchain_images.size(); ++i) {
		VkImageViewCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = m_swapchain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = m_swapchain_image_format;
		create_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;
		VK_CHECK(vkCreateImageView(m_device, &create_info, nullptr, &m_swapchain_image_views[i]));
	}
}

void Context::create_render_pass()
{
	VkAttachmentDescription color_attachment{};
	color_attachment.format = m_swapchain_image_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	VK_CHECK(vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass));
}

void Context::create_framebuffers()
{
	m_swapchain_framebuffers.resize(m_swapchain_image_views.size());

	for (size_t i = 0; i < m_swapchain_image_views.size(); ++i) {
		VkImageView attachments[] = {m_swapchain_image_views[i]};

		VkFramebufferCreateInfo framebuffer_info{};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = m_render_pass;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = attachments;
		framebuffer_info.width = m_swapchain_extent.width;
		framebuffer_info.height = m_swapchain_extent.height;
		framebuffer_info.layers = 1;

		VK_CHECK(vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swapchain_framebuffers[i]));
	}
}

void Context::create_command_buffers()
{
	m_command_buffers.resize(m_swapchain_framebuffers.size());

	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = m_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

	VK_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data()));
}

void Context::create_sync_objects()
{
	m_frames.resize(k_max_frames_in_flight);

	VkSemaphoreCreateInfo semaphore_info{};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (auto& frame : m_frames) {
		VK_CHECK(vkCreateSemaphore(m_device, &semaphore_info, nullptr, &frame.image_available));
		VK_CHECK(vkCreateSemaphore(m_device, &semaphore_info, nullptr, &frame.render_finished));
		VK_CHECK(vkCreateFence(m_device, &fence_info, nullptr, &frame.in_flight));
	}
}

void Context::record_command_buffer(VkCommandBuffer cmd, uint32_t image_index)
{
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

	VkClearValue clear_color{};
	clear_color.color = {{0.05f, 0.10f, 0.12f, 1.0f}};

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = m_render_pass;
	render_pass_info.framebuffer = m_swapchain_framebuffers[image_index];
	render_pass_info.renderArea.offset = {0, 0};
	render_pass_info.renderArea.extent = m_swapchain_extent;
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_color;

	vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));
}

void Context::cleanup_swapchain()
{
	for (auto framebuffer : m_swapchain_framebuffers) vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	m_swapchain_framebuffers.clear();

	if (m_render_pass) vkDestroyRenderPass(m_device, m_render_pass, nullptr);
	m_render_pass = VK_NULL_HANDLE;

	for (auto view : m_swapchain_image_views) vkDestroyImageView(m_device, view, nullptr);
	m_swapchain_image_views.clear();

	if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	m_swapchain = VK_NULL_HANDLE;
}

void Context::recreate_swapchain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0) {
		glfwWaitEvents();
		glfwGetFramebufferSize(m_window, &width, &height);
	}

	vkDeviceWaitIdle(m_device);

	cleanup_swapchain();
	create_swapchain();
	create_image_views();
	create_render_pass();
	create_framebuffers();

	// Command buffers are sized to swapchain images; recreate.
	vkFreeCommandBuffers(m_device, m_command_pool, static_cast<uint32_t>(m_command_buffers.size()), m_command_buffers.data());
	m_command_buffers.clear();
	create_command_buffers();
}

void Context::draw_frame()
{
	auto& frame = m_frames[m_current_frame];

	VK_CHECK(vkWaitForFences(m_device, 1, &frame.in_flight, VK_TRUE, UINT64_MAX));

	uint32_t image_index = 0;
	VkResult acquire_result = vkAcquireNextImageKHR(
	    m_device,
	    m_swapchain,
	    UINT64_MAX,
	    frame.image_available,
	    VK_NULL_HANDLE,
	    &image_index);

	if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreate_swapchain();
		return;
	}
	if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("vkAcquireNextImageKHR failed");
	}

	VK_CHECK(vkResetFences(m_device, 1, &frame.in_flight));

	VK_CHECK(vkResetCommandBuffer(m_command_buffers[image_index], 0));
	record_command_buffer(m_command_buffers[image_index], image_index);

	VkSemaphore wait_semaphores[] = {frame.image_available};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore signal_semaphores[] = {frame.render_finished};

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &m_command_buffers[image_index];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	VK_CHECK(vkQueueSubmit(m_graphics_queue, 1, &submit_info, frame.in_flight));

	VkSwapchainKHR swapchains[] = {m_swapchain};
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;
	present_info.pImageIndices = &image_index;

	VkResult present_result = vkQueuePresentKHR(m_present_queue, &present_info);
	if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR || m_framebuffer_resized) {
		m_framebuffer_resized = false;
		recreate_swapchain();
	} else if (present_result != VK_SUCCESS) {
		throw std::runtime_error("vkQueuePresentKHR failed");
	}

	m_current_frame = (m_current_frame + 1) % k_max_frames_in_flight;
}

} // namespace forest::vk
