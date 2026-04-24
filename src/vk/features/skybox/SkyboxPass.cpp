#include "vk/features/skybox/SkyboxPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/FrameGlobals.hpp"
#include "vk/renderer/helper.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace vkfw
{
  namespace
  {
    static std::vector<char> ReadFile(std::string const &filename)
    {
      std::ifstream file(filename, std::ios::ate | std::ios::binary);
      if (!file.is_open())
        throw std::runtime_error("Failed to open file: " + filename);

      std::vector<char> buffer(static_cast<size_t>(file.tellg()));
      file.seekg(0, std::ios::beg);
      file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      file.close();
      return buffer;
    }
  } // namespace

  bool SkyboxPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();

    // Shaders (generated into build/res/vk by vk_shaders target).
    auto vert_code = ReadFile("res/vk/skybox.vert.spv");
    auto frag_code = ReadFile("res/vk/skybox.frag.spv");

    vk::ShaderModuleCreateInfo vm_ci{};
    vm_ci.codeSize = vert_code.size();
    vm_ci.pCode = reinterpret_cast<uint32_t const *>(vert_code.data());
    vk::raii::ShaderModule vm{device, vm_ci};

    vk::ShaderModuleCreateInfo fm_ci{};
    fm_ci.codeSize = frag_code.size();
    fm_ci.pCode = reinterpret_cast<uint32_t const *>(frag_code.data());
    vk::raii::ShaderModule fm{device, fm_ci};

    vk::PipelineShaderStageCreateInfo stages[2]{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *vm;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *fm;
    stages[1].pName = "main";

    // Descriptor set layout: UBO at set=0,binding=0.
    vk::DescriptorSetLayoutBinding ub_bind{};
    ub_bind.binding = 0;
    ub_bind.descriptorType = vk::DescriptorType::eUniformBuffer;
    ub_bind.descriptorCount = 1;
    ub_bind.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.bindingCount = 1;
    dsl_ci.pBindings = &ub_bind;
    dsl_ = vk::raii::DescriptorSetLayout{device, dsl_ci};

    vk::PipelineLayoutCreateInfo pl_ci{};
    vk::DescriptorSetLayout raw_dsl = *dsl_;
    pl_ci.setLayoutCount = 1;
    pl_ci.pSetLayouts = &raw_dsl;
    pipeline_layout_ = vk::raii::PipelineLayout{device, pl_ci};

    // Pipeline state.
    vk::VertexInputBindingDescription binding{0, sizeof(float) * 3, vk::VertexInputRate::eVertex};
    vk::VertexInputAttributeDescription attr{0, 0, vk::Format::eR32G32B32Sfloat, 0};
    vk::PipelineVertexInputStateCreateInfo vi{};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 1;
    vi.pVertexAttributeDescriptions = &attr;

    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo vp_state{};
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.polygonMode = vk::PolygonMode::eFill;
    rs.cullMode = vk::CullModeFlagBits::eNone;
    rs.frontFace = vk::FrontFace::eClockwise;
    rs.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineDepthStencilStateCreateInfo dss{};
    dss.depthTestEnable = 1;
    dss.depthWriteEnable = 0; // skybox shouldn't write depth
    dss.depthCompareOp = vk::CompareOp::eLessOrEqual;

    vk::PipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                         vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    std::array<vk::DynamicState, 2> dyn{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dy{};
    dy.dynamicStateCount = static_cast<uint32_t>(dyn.size());
    dy.pDynamicStates = dyn.data();

    vk::Format const color_format = swapchain.Format();
    vk::PipelineRenderingCreateInfo rci{};
    rci.colorAttachmentCount = 1;
    rci.pColorAttachmentFormats = &color_format;
    if (targets.shared_depth.Valid())
      rci.depthAttachmentFormat = targets.shared_depth.format;

    vk::GraphicsPipelineCreateInfo gp{};
    gp.pNext = &rci;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp_state;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = targets.shared_depth.Valid() ? &dss : nullptr;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dy;
    gp.layout = *pipeline_layout_;
    pipeline_ = vk::raii::Pipeline{device, nullptr, gp};

    // Vertex buffer (cube).
    static constexpr float kVerts[] = {
        -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f,

        1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,

        -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

    vk::DeviceSize vb_size = sizeof(kVerts);
    vk::BufferCreateInfo vb_ci{};
    vb_ci.size = vb_size;
    vb_ci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    vb_ci.sharingMode = vk::SharingMode::eExclusive;
    vertex_buffer_ = vk::raii::Buffer{device, vb_ci};

    auto req = vertex_buffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo vb_mai{};
    vb_mai.allocationSize = req.size;
    vb_mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), req.memoryTypeBits,
                                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    vertex_memory_ = vk::raii::DeviceMemory{device, vb_mai};
    vertex_buffer_.bindMemory(*vertex_memory_, 0);

    void *dst = vertex_memory_.mapMemory(0, vb_size);
    std::memcpy(dst, kVerts, static_cast<size_t>(vb_size));
    vertex_memory_.unmapMemory();

    // Descriptor pool + sets + per-frame UBOs.
    uint32_t image_count = swapchain.ImageCount();
    vk::DescriptorPoolSize ps{vk::DescriptorType::eUniformBuffer, image_count};
    vk::DescriptorPoolCreateInfo dp_ci{};
    dp_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    dp_ci.maxSets = image_count;
    dp_ci.poolSizeCount = 1;
    dp_ci.pPoolSizes = &ps;
    dp_ = vk::raii::DescriptorPool{device, dp_ci};

    std::vector<vk::DescriptorSetLayout> layouts(image_count, *dsl_);
    vk::DescriptorSetAllocateInfo ds_ai{};
    ds_ai.descriptorPool = *dp_;
    ds_ai.descriptorSetCount = image_count;
    ds_ai.pSetLayouts = layouts.data();
    ds_ = device.allocateDescriptorSets(ds_ai);

    ubo_buf_.reserve(image_count);
    ubo_mem_.reserve(image_count);
    ubo_map_.resize(image_count, nullptr);

    for (uint32_t i = 0; i < image_count; ++i)
    {
      vk::BufferCreateInfo u_ci{};
      u_ci.size = sizeof(SkyboxUBO);
      u_ci.usage = vk::BufferUsageFlagBits::eUniformBuffer;
      ubo_buf_.push_back(vk::raii::Buffer{device, u_ci});

      auto ureq = ubo_buf_.back().getMemoryRequirements();
      vk::MemoryAllocateInfo u_mai{};
      u_mai.allocationSize = ureq.size;
      u_mai.memoryTypeIndex = FindMemoryType(ctx.PhysicalDevice(), ureq.memoryTypeBits,
                                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      ubo_mem_.push_back(vk::raii::DeviceMemory{device, u_mai});
      ubo_buf_.back().bindMemory(*ubo_mem_.back(), 0);

      ubo_map_[i] = ubo_mem_.back().mapMemory(0, sizeof(SkyboxUBO));

      vk::DescriptorBufferInfo bi{*ubo_buf_[i], 0, sizeof(SkyboxUBO)};
      vk::WriteDescriptorSet w{};
      w.dstSet = *ds_[i];
      w.dstBinding = 0;
      w.descriptorCount = 1;
      w.descriptorType = vk::DescriptorType::eUniformBuffer;
      w.pBufferInfo = &bi;
      device.updateDescriptorSets({w}, {});
    }

    return true;
  }

  void SkyboxPass::Destroy(VkContext &ctx)
  {
    ctx.Device().waitIdle();

    pipeline_ = nullptr;
    pipeline_layout_ = nullptr;

    vertex_buffer_ = nullptr;
    vertex_memory_ = nullptr;

    ds_.clear();
    dp_ = nullptr;
    dsl_ = nullptr;

    for (size_t i = 0; i < ubo_map_.size(); ++i)
    {
      if (i < ubo_mem_.size() && ubo_map_[i])
      {
        ubo_mem_[i].unmapMemory();
        ubo_map_[i] = nullptr;
      }
    }
    ubo_map_.clear();
    ubo_mem_.clear();
    ubo_buf_.clear();

    IRenderPass::Destroy(ctx);
  }

  void SkyboxPass::OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    (void)ctx;
    (void)swapchain;
    (void)targets;
    // UBO and pipeline are swapchain-format dependent; a real implementation should recreate here.
    // For now, the demo expects swapchain format to remain stable.
  }

  void SkyboxPass::Record(FrameContext &frame, RenderTargets &targets)
  {
    if (!frame.cmd || !frame.globals)
      return;

    auto &cmd = *frame.cmd;
    uint32_t img = frame.image_index;

    SkyboxUBO ubo{};
    // Remove translation from view.
    glm::mat4 const view_no_trans = glm::mat4(glm::mat3(frame.globals->view));
    ubo.world_to_clip = frame.globals->proj * view_no_trans;
    ubo.light_position = glm::vec4(frame.globals->light_position, 0.0f);
    ubo.time_seconds = frame.globals->time_seconds;

    if (img < ubo_map_.size() && ubo_map_[img])
      std::memcpy(ubo_map_[img], &ubo, sizeof(ubo));

    vk::ClearValue clear_color{};
    clear_color.color = vk::ClearColorValue(std::array<float, 4>{{0.0f, 0.0f, 0.0f, 1.0f}});
    vk::RenderingAttachmentInfo color_att{};
    color_att.imageView = frame.swapchain_image_view;
    color_att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att.loadOp = vk::AttachmentLoadOp::eClear;
    color_att.storeOp = vk::AttachmentStoreOp::eStore;
    color_att.clearValue = clear_color;

    vk::RenderingInfo ri{};
    ri.renderArea.offset = vk::Offset2D{0, 0};
    ri.renderArea.extent = frame.swapchain_extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &color_att;

    vk::RenderingAttachmentInfo depth_att{};
    if (targets.shared_depth.Valid())
    {
      depth_att.imageView = targets.shared_depth.view;
      depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
      depth_att.loadOp = vk::AttachmentLoadOp::eClear;
      depth_att.storeOp = vk::AttachmentStoreOp::eStore;
      depth_att.clearValue = vk::ClearValue(vk::ClearDepthStencilValue{1.0f, 0});
      ri.pDepthAttachment = &depth_att;
    }

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    cmd.setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(frame.swapchain_extent.width),
                                    static_cast<float>(frame.swapchain_extent.height), 0.0f, 1.0f});
    cmd.setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, frame.swapchain_extent});

    cmd.bindVertexBuffers(0, *vertex_buffer_, {0});
    if (img < ds_.size())
    {
      cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout_, 0, {*ds_[img]}, {});
    }
    cmd.draw(36, 1, 0, 0);
    cmd.endRendering();
  }

} // namespace vkfw
