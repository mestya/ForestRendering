#include "vk/core/VkContext.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <stdexcept>

namespace vkfw {
namespace {

static std::vector<char const*> ValidationLayers()
{
  return {"VK_LAYER_KHRONOS_validation"};
}

static std::vector<char const*> RequiredDeviceExtensions()
{
  return {vk::KHRSwapchainExtensionName};
}

static std::vector<char const*> GetRequiredInstanceExtensions(bool enable_validation)
{
  uint32_t glfw_count = 0;
  char const** glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_count);
  if (glfw_ext == nullptr || glfw_count == 0)
    throw std::runtime_error("glfwGetRequiredInstanceExtensions failed");

  std::vector<char const*> exts(glfw_ext, glfw_ext + glfw_count);
  if (enable_validation)
    exts.push_back(vk::EXTDebugUtilsExtensionName);
  return exts;
}

static VKAPI_ATTR VkBool32 VKAPI_PTR DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                   vk::DebugUtilsMessageTypeFlagsEXT,
                                                   vk::DebugUtilsMessengerCallbackDataEXT const* cb,
                                                   void*)
{
  if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning ||
      severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
    // Keep it minimal; upstream logging can be added later.
    if (cb && cb->pMessage)
      std::fprintf(stderr, "validation: %s\n", cb->pMessage);
  }
  return VK_FALSE;
}

static bool HasExtension(std::vector<vk::ExtensionProperties> const& props, char const* name)
{
  return std::any_of(props.begin(), props.end(), [&](auto const& p) {
    return std::strcmp(p.extensionName, name) == 0;
  });
}

static bool HasLayer(std::vector<vk::LayerProperties> const& props, char const* name)
{
  return std::any_of(props.begin(), props.end(), [&](auto const& p) {
    return std::strcmp(p.layerName, name) == 0;
  });
}

} // namespace

class VkContext::Impl {
public:
  bool initialized = false;
  GLFWwindow* window = nullptr;
  bool enable_validation = true;

  vk::raii::Context context{};
  vk::raii::Instance instance{nullptr};
  vk::raii::DebugUtilsMessengerEXT debug_messenger{nullptr};
  vk::raii::SurfaceKHR surface{nullptr};
  vk::raii::PhysicalDevice physical_device{nullptr};
  vk::raii::Device device{nullptr};
  uint32_t graphics_queue_family_index = 0;
  vk::raii::Queue graphics_queue{nullptr};
};

VkContext::VkContext() : impl_(new Impl()) {}
VkContext::~VkContext() = default;
VkContext::VkContext(VkContext&&) noexcept = default;
VkContext& VkContext::operator=(VkContext&&) noexcept = default;

static uint32_t FindGraphicsPresentQueueFamily(vk::raii::PhysicalDevice const& pd, vk::SurfaceKHR surface)
{
  auto qfps = pd.getQueueFamilyProperties();
  for (uint32_t i = 0; i < static_cast<uint32_t>(qfps.size()); ++i) {
    if (!(qfps[i].queueFlags & vk::QueueFlagBits::eGraphics))
      continue;
    if (pd.getSurfaceSupportKHR(i, surface))
      return i;
  }
  return ~0u;
}

static bool DeviceSuitable(vk::raii::PhysicalDevice const& pd, vk::SurfaceKHR surface)
{
  if (pd.getProperties().apiVersion < VK_API_VERSION_1_3)
    return false;

  if (FindGraphicsPresentQueueFamily(pd, surface) == ~0u)
    return false;

  auto exts = pd.enumerateDeviceExtensionProperties();
  for (auto const* req : RequiredDeviceExtensions()) {
    if (!HasExtension(exts, req))
      return false;
  }

  auto feats = pd.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features>();
  auto const& f13 = feats.get<vk::PhysicalDeviceVulkan13Features>();
  if (!f13.dynamicRendering || !f13.synchronization2)
    return false;

  return true;
}

bool VkContext::Init(ContextCreateInfo const& info)
{
  if (info.window == nullptr)
    throw std::runtime_error("vkfw::VkContext: window is null");

  impl_->window = info.window;
  impl_->enable_validation = info.enable_validation;

  // Initialize the default dispatcher for dynamic dispatch mode.
  VULKAN_HPP_DEFAULT_DISPATCHER.init(::vkGetInstanceProcAddr);

  // Instance creation.
  vk::ApplicationInfo app_info{};
  app_info.pApplicationName = "ForestRenderingVk";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "vkfw";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_3;

  std::vector<char const*> layers{};
  if (impl_->enable_validation) {
    auto avail_layers = impl_->context.enumerateInstanceLayerProperties();
    for (auto const* layer : ValidationLayers()) {
      if (!HasLayer(avail_layers, layer))
        throw std::runtime_error(std::string("Missing validation layer: ") + layer);
    }
    layers = ValidationLayers();
  }

  auto exts = GetRequiredInstanceExtensions(impl_->enable_validation);
  auto avail_exts = impl_->context.enumerateInstanceExtensionProperties();
  for (auto const* ext : exts) {
    if (!HasExtension(avail_exts, ext))
      throw std::runtime_error(std::string("Missing instance extension: ") + ext);
  }

  vk::InstanceCreateInfo ici{};
  ici.pApplicationInfo = &app_info;
  ici.enabledLayerCount = static_cast<uint32_t>(layers.size());
  ici.ppEnabledLayerNames = layers.data();
  ici.enabledExtensionCount = static_cast<uint32_t>(exts.size());
  ici.ppEnabledExtensionNames = exts.data();
  impl_->instance = vk::raii::Instance{impl_->context, ici};
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*impl_->instance);

  if (impl_->enable_validation) {
    vk::DebugUtilsMessengerCreateInfoEXT dci{};
    dci.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    dci.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    dci.pfnUserCallback = DebugCallback;
    impl_->debug_messenger = impl_->instance.createDebugUtilsMessengerEXT(dci);
  }

  // Surface.
  VkSurfaceKHR raw_surface{};
  if (glfwCreateWindowSurface(*impl_->instance, impl_->window, nullptr, &raw_surface) != VK_SUCCESS)
    throw std::runtime_error("glfwCreateWindowSurface failed");
  impl_->surface = vk::raii::SurfaceKHR{impl_->instance, raw_surface};

  // Physical device.
  auto pds = impl_->instance.enumeratePhysicalDevices();
  auto it = std::find_if(pds.begin(), pds.end(), [&](auto const& pd) {
    return DeviceSuitable(pd, *impl_->surface);
  });
  if (it == pds.end())
    throw std::runtime_error("No suitable physical device found");
  impl_->physical_device = *it;

  impl_->graphics_queue_family_index =
      FindGraphicsPresentQueueFamily(impl_->physical_device, *impl_->surface);
  if (impl_->graphics_queue_family_index == ~0u)
    throw std::runtime_error("No graphics+present queue family found");

  // Device.
  float priority = 1.0f;
  vk::DeviceQueueCreateInfo qci{};
  qci.queueFamilyIndex = impl_->graphics_queue_family_index;
  qci.queueCount = 1;
  qci.pQueuePriorities = &priority;

  vk::PhysicalDeviceVulkan13Features f13{};
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;
  vk::PhysicalDeviceFeatures2 f2{};
  f2.pNext = &f13;

  auto dev_exts = RequiredDeviceExtensions();
  vk::DeviceCreateInfo dci{};
  dci.pNext = &f2;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.enabledExtensionCount = static_cast<uint32_t>(dev_exts.size());
  dci.ppEnabledExtensionNames = dev_exts.data();
  impl_->device = vk::raii::Device{impl_->physical_device, dci};
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*impl_->device);

  impl_->graphics_queue = vk::raii::Queue{impl_->device, impl_->graphics_queue_family_index, 0};

  impl_->initialized = true;
  return true;
}

void VkContext::Shutdown()
{
  impl_->graphics_queue = nullptr;
  impl_->device = nullptr;
  impl_->physical_device = nullptr;
  impl_->surface = nullptr;
  impl_->debug_messenger = nullptr;
  impl_->instance = nullptr;
  impl_->window = nullptr;
  impl_->initialized = false;
}

bool VkContext::IsInitialized() const noexcept { return impl_->initialized; }

vk::raii::Context& VkContext::Context() const { return impl_->context; }
vk::raii::Instance& VkContext::Instance() const { return impl_->instance; }
vk::raii::SurfaceKHR& VkContext::Surface() const { return impl_->surface; }
::GLFWwindow* VkContext::Window() const noexcept { return impl_->window; }
vk::raii::PhysicalDevice& VkContext::PhysicalDevice() const { return impl_->physical_device; }
vk::raii::Device& VkContext::Device() const { return impl_->device; }
vk::raii::Queue& VkContext::GraphicsQueue() const { return impl_->graphics_queue; }
uint32_t VkContext::GraphicsQueueFamilyIndex() const noexcept { return impl_->graphics_queue_family_index; }

} // namespace vkfw
