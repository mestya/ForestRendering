#include "vk/features/ui/ImGuiPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/renderer/helper.hpp"
#include <imgui_impl_glfw.h>
#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include<iostream>

namespace vkfw {

bool ImGuiPass::Create(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets)
{
  
  InitImGuiDynamic(ctx,swapchain,targets);
  return true;
}

void ImGuiPass::InitImGuiDynamic(VkContext& ctx, VkSwapchain const& swapchain, RenderTargets& targets) 
{
    // --- 1. 数据准备 ---
    std::cout<<"start"<<std::endl;
    auto window              = ctx.Window();
    const auto& instance     = ctx.Instance(); 
    const auto& device       = ctx.Device();
    const auto& physicalDevice = ctx.PhysicalDevice();
    const auto& graphicsQueue  = ctx.GraphicsQueue();
    auto queueFamilyIndex    = ctx.GraphicsQueueFamilyIndex();
    
    vk::Format swapChainFormat = swapchain.SurfaceFormat().format; 
    vk::Format depthFormat     = vk::Format::eD32Sfloat;
    uint32_t imageCount        = swapchain.ImageCount();

    // --- 2. 创建 RAII 描述符池 ---
    // ImGui 后端会分配多种类型的 descriptor，并且对 image sampler 数量有下限断言（见 IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE）。
    // 这里直接给一个足够大的通用池，避免初始化阶段 IM_ASSERT 直接 abort。
    std::array<vk::DescriptorPoolSize, 11> poolSizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1000},
        vk::DescriptorPoolSize{vk::DescriptorType::eInputAttachment, 1000},
    };

    vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo()
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(1000 * static_cast<uint32_t>(poolSizes.size()))
        .setPoolSizeCount(static_cast<uint32_t>(poolSizes.size()))
        .setPPoolSizes(poolSizes.data());

    imu_resources_.descriptorPool = vk::raii::DescriptorPool(device, poolInfo);

    // --- 3. ImGui 核心上下文设置 ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // --- 4. 初始化 GLFW 后端 ---
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // --- 5. 初始化 Vulkan 后端 (动态渲染模式) ---
    
  ImGui_ImplVulkan_InitInfo initInfo = {};
  // 1. 强制清零整个结构体，防止任何隐藏字段（如 ApiVersion）是随机值
  memset(&initInfo, 0, sizeof(initInfo)); 

  // 0. 如果工程启用了 IMGUI_IMPL_VULKAN_NO_PROTOTYPES/VK_NO_PROTOTYPES，必须先加载函数指针，否则后端会 IM_ASSERT 直接终止。
  // external/CMakeLists.txt 里对 external_libs 设置了 IMGUI_IMPL_VULKAN_NO_PROTOTYPES（PUBLIC），所以这里必需。
  // auto loader = [](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
  //   VkInstance instance = reinterpret_cast<VkInstance>(user_data);
  //   if (auto fn = ::vkGetInstanceProcAddr(instance, function_name))
  //     return fn;
  //   return ::vkGetInstanceProcAddr(VK_NULL_HANDLE, function_name);
  // };
  // if (!ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, loader, (void*)(VkInstance)*ctx.Instance())) {
  //   throw std::runtime_error("ImGui_ImplVulkan_LoadFunctions failed");
  // }

  // 2. 基础信息
  initInfo.ApiVersion     = VK_API_VERSION_1_3;
  initInfo.Instance       = *ctx.Instance();
  initInfo.PhysicalDevice = *ctx.PhysicalDevice();
  initInfo.Device         = *ctx.Device();
  initInfo.QueueFamily    = queueFamilyIndex;
  initInfo.Queue          = *graphicsQueue;
  initInfo.DescriptorPool = *imu_resources_.descriptorPool;
  initInfo.MinImageCount  = 2;
  initInfo.ImageCount     = imageCount;

  // 3. 动态渲染配置
  initInfo.UseDynamicRendering = true;
  initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  // 重点：手动设置 sType 
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  imgui_color_format_ = static_cast<VkFormat>(swapChainFormat);
  initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &imgui_color_format_;
  initInfo.CheckVkResultFn = [](VkResult err) {
    if (err != VK_SUCCESS)
      std::cerr << "[ImGui] Vulkan Error: " << static_cast<int>(err) << std::endl;
  };
  if (nullptr==imu_resources_.descriptorPool) {
    throw std::runtime_error("描述符池创建失败！");
  }
// 使用 static_cast 转换为底层句柄类型，再转为 void* 方便 cout 识别
std::cout << "[DEBUG] Instance: "       << (void*)(VkInstance)*ctx.Instance() << std::endl;
std::cout << "[DEBUG] PhysicalDevice: " << (void*)(VkPhysicalDevice)*ctx.PhysicalDevice() << std::endl;
std::cout << "[DEBUG] Device: "         << (void*)(VkDevice)*ctx.Device() << std::endl;
std::cout << "[DEBUG] Size of InitInfo: " << (uint32_t)sizeof(ImGui_ImplVulkan_InitInfo) << std::endl;
// 结构体内部的已经是原始类型了，直接转 void*
std::cout << "[DEBUG] initInfo PD: "    << (void*)initInfo.PhysicalDevice << std::endl;
    ImGui_ImplVulkan_Init(&initInfo);
  std::cout<<"end"<<std::endl;
    // --- 6. 创建字体 ---
    // ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiPass::Destroy(VkContext& ctx) {
    ctx.Device().waitIdle();
    debugParameter_ = nullptr;
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imu_resources_.descriptorPool = nullptr;
    imgui_color_format_ = VK_FORMAT_UNDEFINED;
    IRenderPass::Destroy(ctx);
}

void ImGuiPass::OnSwapchainRecreated(VkContext&, VkSwapchain const&, RenderTargets&) {}

void ImGuiPass::Record(FrameContext& frame, RenderTargets& targets) {
  
    if (frame.cmd == nullptr)
      return;

    auto& cmd = *frame.cmd;

    vk::ClearValue clear{};
    clear.color = vk::ClearColorValue(std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}});

    vk::RenderingAttachmentInfo att{};
    att.imageView = frame.swapchain_image_view;
    att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    att.loadOp = vk::AttachmentLoadOp::eLoad;
    att.storeOp = vk::AttachmentStoreOp::eStore;
    att.clearValue = clear;

    vk::RenderingInfo ri{};
    ri.renderArea.offset = vk::Offset2D{0, 0};
    ri.renderArea.extent = frame.swapchain_extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &att;

      // 2. 启动 ImGui 帧
      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      // 3. 定义 UI 界面逻辑 (这就是你创建按钮的地方)
      {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);  
        ImGui::Begin("debug interface"); // 创建一个窗口
          
          ImGui::Text("number of trees: 1024");
          
          // // 创建按钮：Button 返回 true 代表本帧被点击了
          // if (ImGui::Button("生成新随机森林")) {
          //     // 这里写点击按钮后要执行的逻辑
          //     std::cout << "正在重新种植森林..." << std::endl;
          // }

          static float treeScale = 1.0f;
          if(debugParameter_){
              ImGui::Checkbox("Animation", &debugParameter_->animation);
          }
          
          ImGui::SliderFloat("scale", &treeScale, 0.1f, 5.0f);
          // ImGui::Checkbox("rotation", &_isStart);
          ImGui::End();
          ImGui::Render();
      }

    cmd.beginRendering(ri);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd);

    cmd.endRendering();

    // Final transition back to Present for vkQueuePresentKHR.
    TransitionImage(cmd,
                    frame.swapchain_image,
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR,
                    vk::ImageAspectFlagBits::eColor,
                    vk::AccessFlagBits2::eColorAttachmentWrite,
                    {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits2::eBottomOfPipe);
    frame.swapchain_old_layout = vk::ImageLayout::ePresentSrcKHR;

}

void ImGuiPass::setDebugParameter(DebugParam& param){
  debugParameter_=&param;
}

} // namespace vkfw
