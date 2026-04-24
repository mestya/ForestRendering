#include "vk/features/lighting/LightingPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace vkfw
{
  namespace
  {

    static vk::VertexInputBindingDescription BindingDesc()
    {
      return vk::VertexInputBindingDescription{0u, sizeof(VertexTest), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> AttrDescs()
    {
      return {
          vk::VertexInputAttributeDescription{0u, 0u, vk::Format::eR32G32Sfloat, offsetof(VertexTest, pos)},
          vk::VertexInputAttributeDescription{1u, 0u, vk::Format::eR32G32B32Sfloat, offsetof(VertexTest, color)},
      };
    }

  } // namespace

  bool LightingPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &)
  {
    auto &device = ctx.Device();

    // Shaders: reuse the existing demo SPIR-V that contains vertMain/fragMain.
    auto const code = ReadFile("res/09_shader_base.spv");
    vk::ShaderModuleCreateInfo sm_ci{};
    sm_ci.codeSize = code.size();
    sm_ci.pCode = reinterpret_cast<uint32_t const *>(code.data());
    vk::raii::ShaderModule shader_module{device, sm_ci};

    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *shader_module;
    stages[0].pName = "vertMain";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *shader_module;
    stages[1].pName = "fragMain";

    auto binding = BindingDesc();
    auto attrs = AttrDescs();
    vk::PipelineVertexInputStateCreateInfo vi{};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo vp_state{};
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.polygonMode = vk::PolygonMode::eFill;
    rs.cullMode = vk::CullModeFlagBits::eBack;
    rs.frontFace = vk::FrontFace::eClockwise;
    rs.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState cb_att{};
    cb_att.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 1;
    cb.pAttachments = &cb_att;

    std::array<vk::DynamicState, 2> dyn{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dyn_ci{};
    dyn_ci.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dyn_ci.pDynamicStates = dyn.data();

    vk::PushConstantRange pcr{};
    pcr.stageFlags = vk::ShaderStageFlagBits::eFragment;
    pcr.offset = 0;
    pcr.size = sizeof(float);

    vk::PipelineLayoutCreateInfo pl_ci{};
    pl_ci.pushConstantRangeCount = 1;
    pl_ci.pPushConstantRanges = &pcr;
    pipeline_layout_ = vk::raii::PipelineLayout{device, pl_ci};

    vk::Format const color_format = swapchain.Format();

    vk::PipelineRenderingCreateInfo rendering_ci{};
    rendering_ci.colorAttachmentCount = 1;
    rendering_ci.pColorAttachmentFormats = &color_format;

    vk::GraphicsPipelineCreateInfo gp_ci{};
    gp_ci.pNext = &rendering_ci;
    gp_ci.stageCount = 2;
    gp_ci.pStages = stages;
    gp_ci.pVertexInputState = &vi;
    gp_ci.pInputAssemblyState = &ia;
    gp_ci.pViewportState = &vp_state;
    gp_ci.pRasterizationState = &rs;
    gp_ci.pMultisampleState = &ms;
    gp_ci.pColorBlendState = &cb;
    gp_ci.pDynamicState = &dyn_ci;
    gp_ci.layout = *pipeline_layout_;
    gp_ci.renderPass = nullptr;

    pipeline_ = vk::raii::Pipeline{device, nullptr, gp_ci};

    // Geometry: a single triangle.
    vertices_ = {{
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    }};
    std::array<uint16_t, 3> const indices = {{0, 1, 2}};

    auto const mem_props = ctx.PhysicalDevice().getMemoryProperties();

    {
      vk::BufferCreateInfo bci{};
      bci.size = sizeof(vertices_);
      bci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
      bci.sharingMode = vk::SharingMode::eExclusive;
      vertex_buffer_ = vk::raii::Buffer{device, bci};

      auto req = vertex_buffer_.getMemoryRequirements();
      vk::MemoryAllocateInfo mai{};
      mai.allocationSize = req.size;
      mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      vertex_memory_ = vk::raii::DeviceMemory{device, mai};
      vertex_buffer_.bindMemory(*vertex_memory_, 0);

      void *dst = vertex_memory_.mapMemory(0, bci.size);
      std::memcpy(dst, vertices_.data(), sizeof(vertices_));
      vertex_memory_.unmapMemory();
    }

    {
      vk::BufferCreateInfo bci{};
      bci.size = sizeof(indices);
      bci.usage = vk::BufferUsageFlagBits::eIndexBuffer;
      bci.sharingMode = vk::SharingMode::eExclusive;
      index_buffer_ = vk::raii::Buffer{device, bci};

      auto req = index_buffer_.getMemoryRequirements();
      vk::MemoryAllocateInfo mai{};
      mai.allocationSize = req.size;
      mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      index_memory_ = vk::raii::DeviceMemory{device, mai};
      index_buffer_.bindMemory(*index_memory_, 0);

      void *dst = index_memory_.mapMemory(0, bci.size);
      std::memcpy(dst, indices.data(), sizeof(indices));
      index_memory_.unmapMemory();
    }

    return true;
  }

  void LightingPass::Destroy(VkContext &)
  {
    // raii handles clean themselves up
    debugParameter_ = nullptr;
    pipeline_ = nullptr;
    pipeline_layout_ = nullptr;
    index_buffer_ = nullptr;
    index_memory_ = nullptr;
    vertex_buffer_ = nullptr;
    vertex_memory_ = nullptr;
    // IRenderPass::Destroy(ctx);
  }

  void LightingPass::OnSwapchainRecreated(VkContext &, VkSwapchain const &, RenderTargets &)
  {
    // For the minimal triangle, nothing swapchain-sized is owned here.
  }

  void LightingPass::Record(FrameContext &frame, RenderTargets &)
  {
    if (debugParameter_ && debugParameter_->animation)
    {
      updateVertexBuffer();
    }

    if (frame.cmd == nullptr)
      return;

    auto &cmd = *frame.cmd;

    // Transition swapchain image for rendering.
    TransitionImage(cmd, frame.swapchain_image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                    vk::ImageAspectFlagBits::eColor, vk::AccessFlagBits2::eColorAttachmentWrite, {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);

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

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(frame.swapchain_extent.width),
                                    static_cast<float>(frame.swapchain_extent.height), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, frame.swapchain_extent});
    cmd.bindVertexBuffers(0, *vertex_buffer_, {0});
    cmd.bindIndexBuffer(*index_buffer_, 0, vk::IndexType::eUint16);

    float t = 0.0f;
    cmd.pushConstants<float>(*pipeline_layout_, vk::ShaderStageFlagBits::eFragment, 0, t);
    cmd.drawIndexed(3, 1, 0, 0, 0);

    cmd.endRendering();

    // Transition to present.
    TransitionImage(cmd, frame.swapchain_image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                    vk::ImageAspectFlagBits::eColor, vk::AccessFlagBits2::eColorAttachmentWrite, {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);
  }

  void LightingPass::setDebugParameter(DebugParam &param)
  {
    debugParameter_ = &param;
  }

  void LightingPass::updateVertexBuffer()
  {
    static float dir = 1.0f;

    vertices_[0].pos[0] += 0.01f * dir;

    if (vertices_[0].pos[0] >= 1.0f)
    {
      vertices_[0].pos[0] = 1.0f;
      dir = -1.0f;
    }
    else if (vertices_[0].pos[0] <= -1.0f)
    {
      vertices_[0].pos[0] = -1.0f;
      dir = 1.0f;
    }
    void *data = vertex_memory_.mapMemory(0, sizeof(vertices_[0]) * vertices_.size());
    memcpy(data, vertices_.data(), sizeof(vertices_[0]) * vertices_.size());
    vertex_memory_.unmapMemory();
  }

} // namespace vkfw
