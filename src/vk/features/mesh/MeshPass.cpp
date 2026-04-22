#include "vk/features/mesh/MeshPass.hpp"

#include "vk/core/VkContext.hpp"
#include "vk/core/VkSwapchain.hpp"
#include "vk/renderer/FrameContext.hpp"
#include "vk/renderer/FrameGlobals.hpp"
#include "vk/renderer/RenderTargets.hpp"
#include "vk/scene/Model.hpp"
#include "vk/scene/Vertex.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <stdexcept>

// #define STB_IMAGE_IMPLEMENTATION
#include "vk/scene/stb_image.h"

#include <random>
#include <iostream>

namespace vkfw
{
  // namespace
  // {
  //   static void TransitionImage(vk::raii::CommandBuffer &cmd,
  //                               vk::Image image,
  //                               vk::ImageLayout old_layout,
  //                               vk::ImageLayout new_layout,
  //                               vk::ImageAspectFlags aspect,
  //                               vk::AccessFlags2 src_access,
  //                               vk::AccessFlags2 dst_access,
  //                               vk::PipelineStageFlags2 src_stage,
  //                               vk::PipelineStageFlags2 dst_stage)
  //   {
  //     vk::ImageMemoryBarrier2 barrier{};
  //     barrier.srcStageMask = src_stage;
  //     barrier.srcAccessMask = src_access;
  //     barrier.dstStageMask = dst_stage;
  //     barrier.dstAccessMask = dst_access;
  //     barrier.oldLayout = old_layout;
  //     barrier.newLayout = new_layout;
  //     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  //     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  //     barrier.image = image;
  //     barrier.subresourceRange.aspectMask = aspect;
  //     barrier.subresourceRange.baseMipLevel = 0;
  //     barrier.subresourceRange.levelCount = 1;
  //     barrier.subresourceRange.baseArrayLayer = 0;
  //     barrier.subresourceRange.layerCount = 1;

  //     vk::DependencyInfo dep{};
  //     dep.imageMemoryBarrierCount = 1;
  //     dep.pImageMemoryBarriers = &barrier;
  //     cmd.pipelineBarrier2(dep);
  //   }

  // } // namespace

  MeshPass::MeshPass(std::string model_path) : model_path_(std::move(model_path)) {}

  std::vector<char> MeshPass::ReadFile(std::string const &filename)
  {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
      throw std::runtime_error("Failed to open file: " + filename);
    std::vector<char> buffer(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
  }

  uint32_t MeshPass::FindMemoryType(VkContext &ctx, uint32_t type_bits, vk::MemoryPropertyFlags required)
  {
    auto mem_props = ctx.PhysicalDevice().getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
    {
      if ((type_bits & (1u << i)) == 0)
        continue;
      if ((mem_props.memoryTypes[i].propertyFlags & required) == required)
        return i;
    }
    throw std::runtime_error("No suitable memory type");
  }

  bool MeshPass::Create(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &targets)
  {
    auto &device = ctx.Device();
    sub_meshes_.clear();

    // 1. 分别加载模型零件
    Model stemModel = Model::LoadFromFile("res/47-mapletree/MapleTreeStem.obj");
    Model leafModel = Model::LoadFromFile("res/47-mapletree/MapleTreeLeaves.obj");

    // 记录子网格信息
    sub_meshes_.push_back({(uint32_t)stemModel.indices.size(), 0, 0});
    sub_meshes_.push_back({(uint32_t)leafModel.indices.size(), (uint32_t)stemModel.indices.size(), 1});

    // 合并顶点
    std::vector<Vertex> all_vertices = stemModel.vertices;
    all_vertices.insert(all_vertices.end(), leafModel.vertices.begin(), leafModel.vertices.end());

    // 合并索引 (处理偏移)
    std::vector<uint32_t> all_indices = stemModel.indices;
    uint32_t stemVertexOffset = static_cast<uint32_t>(stemModel.vertices.size());
    for (auto idx : leafModel.indices)
    {
      all_indices.push_back(idx + stemVertexOffset);
    }
    total_index_count_ = static_cast<uint32_t>(all_indices.size());
    //   glm::mat4 t = glm::translate(glm::mat4{1.0f}, -stemModel.center);

    //   // 关键：这里乘以 0.1f 缩小 10 倍，这样它就不会怼到相机脸上了
    //   float scale_factor = 0.5f / (stemModel.radius > 0.0f ? stemModel.radius : 1.0f);
    //   glm::mat4 s = glm::scale(glm::mat4{1.0f}, glm::vec3{scale_factor});

    //   model_matrix_ = s * t;
    std::default_random_engine generator;
    std::uniform_real_distribution<float> pos_dist(-5.0f, 5.0f);  // 位置微调偏移
    std::uniform_real_distribution<float> rot_dist(0.0f, 360.0f); // 随机旋转角度
    std::uniform_real_distribution<float> scale_dist(0.8f, 1.5f); // 随机大小缩放

    // 1. 生成 10x10 的森林矩阵
    std::vector<InstanceData> instance_matrices;
    int count = 10;        // 10x10 的森林
    float spacing = 30.0f; // 树木间距
    for (int x = -count; x < count; x++)
    {
      for (int z = -count; z < count; z++)
      {
        InstanceData data;

        // 1. 基础位置 + 随机偏移
        float posX = x * spacing + pos_dist(generator);
        float posZ = z * spacing + pos_dist(generator);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(posX, 0.0f, posZ));

        // 2. 随机绕 Y 轴旋转（树木在自然界中朝向各异）
        m = glm::rotate(m, glm::radians(rot_dist(generator)), glm::vec3(0, 1, 0));

        // 3. 随机缩放（让树有高有矮，有胖有瘦）
        float s = scale_dist(generator);
        m = glm::scale(m, glm::vec3(s));

        data.model = m;
        instance_matrices.push_back(data);
      }
    }
    instance_count_ = static_cast<uint32_t>(instance_matrices.size());

    // 2. 创建 Instance Buffer
    vk::DeviceSize buffer_size = sizeof(InstanceData) * instance_matrices.size();
    vk::BufferCreateInfo inst_ci{};
    inst_ci.size = buffer_size;
    inst_ci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    instance_buf_ = vk::raii::Buffer{device, inst_ci};

    auto inst_req = instance_buf_.getMemoryRequirements();
    vk::MemoryAllocateInfo inst_mai{};
    inst_mai.sType = vk::StructureType::eMemoryAllocateInfo; // 必须显式指定
    inst_mai.allocationSize = inst_req.size;
    inst_mai.memoryTypeIndex = FindMemoryType(ctx, inst_req.memoryTypeBits,
                                              vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    instance_mem_ = vk::raii::DeviceMemory{device, inst_mai};
    instance_buf_.bindMemory(*instance_mem_, 0);

    // 3. 拷贝数据
    void *p_data = instance_mem_.mapMemory(0, buffer_size);
    std::memcpy(p_data, instance_matrices.data(), (size_t)buffer_size);
    instance_mem_.unmapMemory();

    // 2. 创建 VB (展开写法，避免类型匹配报错)
    {
      vk::DeviceSize vb_size = sizeof(Vertex) * all_vertices.size();
      vk::BufferCreateInfo vb_ci{};
      vb_ci.size = vb_size;
      vb_ci.usage = vk::BufferUsageFlagBits::eVertexBuffer;
      vb_ci.sharingMode = vk::SharingMode::eExclusive;
      vb_ = vk::raii::Buffer{device, vb_ci};

      auto req = vb_.getMemoryRequirements();
      vk::MemoryAllocateInfo vb_mai{};
      vb_mai.allocationSize = req.size;
      vb_mai.memoryTypeIndex = FindMemoryType(ctx, req.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      vb_mem_ = vk::raii::DeviceMemory{device, vb_mai};
      vb_.bindMemory(*vb_mem_, 0);

      void *m = vb_mem_.mapMemory(0, vb_size);
      std::memcpy(m, all_vertices.data(), (size_t)vb_size);
      vb_mem_.unmapMemory();
    }

    // 创建 IB
    {
      vk::DeviceSize ib_size = sizeof(uint32_t) * all_indices.size();
      vk::BufferCreateInfo ib_ci{};
      ib_ci.size = ib_size;
      ib_ci.usage = vk::BufferUsageFlagBits::eIndexBuffer;
      ib_ci.sharingMode = vk::SharingMode::eExclusive;
      ib_ = vk::raii::Buffer{device, ib_ci};

      auto ireq = ib_.getMemoryRequirements();
      vk::MemoryAllocateInfo ib_mai{};
      ib_mai.allocationSize = ireq.size;
      ib_mai.memoryTypeIndex = FindMemoryType(ctx, ireq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      ib_mem_ = vk::raii::DeviceMemory{device, ib_mai};
      ib_.bindMemory(*ib_mem_, 0);

      void *im = ib_mem_.mapMemory(0, ib_size);
      std::memcpy(im, all_indices.data(), (size_t)ib_size);
      ib_mem_.unmapMemory();
    }

    // 3. 深度与贴图
    if (!targets.shared_depth.Valid())
      throw std::runtime_error("MeshPass requires RenderTargets::shared_depth");
    LoadTexture(ctx, "res/47-mapletree/maple_bark.png", 0);

    LoadTexture(ctx, "res/47-mapletree/maple_leaf.png", 1);

    CreateCommonSampler(device);

    // 4. Descriptor Layout
    vk::DescriptorSetLayoutBinding ub_bind{};
    ub_bind.binding = 0;
    ub_bind.descriptorType = vk::DescriptorType::eUniformBuffer;
    ub_bind.descriptorCount = 1;
    ub_bind.stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::DescriptorSetLayoutBinding smp_bind{};
    smp_bind.binding = 1;
    smp_bind.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    smp_bind.descriptorCount = 1;
    smp_bind.stageFlags = vk::ShaderStageFlagBits::eFragment;

    std::array<vk::DescriptorSetLayoutBinding, 2> binds = {ub_bind, smp_bind};
    vk::DescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.bindingCount = 2;
    dsl_ci.pBindings = binds.data();
    dsl_ = vk::raii::DescriptorSetLayout{device, dsl_ci};

    // 5. Descriptor Pool
    uint32_t image_count = swapchain.ImageCount();
    uint32_t total_sets = image_count * 2;
    std::array<vk::DescriptorPoolSize, 2> ps{};
    ps[0] = {vk::DescriptorType::eUniformBuffer, total_sets};
    ps[1] = {vk::DescriptorType::eCombinedImageSampler, total_sets};

    vk::DescriptorPoolCreateInfo dp_ci{};
    dp_ci.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    dp_ci.maxSets = total_sets;
    dp_ci.poolSizeCount = 2;
    dp_ci.pPoolSizes = ps.data();
    dp_ = vk::raii::DescriptorPool{device, dp_ci};

    std::vector<vk::DescriptorSetLayout> layouts(total_sets, *dsl_);
    vk::DescriptorSetAllocateInfo ds_ai{};
    ds_ai.descriptorPool = *dp_;
    ds_ai.descriptorSetCount = total_sets;
    ds_ai.pSetLayouts = layouts.data();
    ds_ = device.allocateDescriptorSets(ds_ai);

    // 6. UBO 与 更新
    ubo_buf_.reserve(image_count);
    ubo_mem_.reserve(image_count);
    ubo_map_.resize(image_count, nullptr);

    for (uint32_t i = 0; i < image_count; ++i)
    {
      vk::BufferCreateInfo u_ci{};
      u_ci.size = sizeof(CameraUBO);
      u_ci.usage = vk::BufferUsageFlagBits::eUniformBuffer;
      ubo_buf_.push_back(vk::raii::Buffer{device, u_ci});

      auto req = ubo_buf_.back().getMemoryRequirements();
      vk::MemoryAllocateInfo u_mai{};
      u_mai.allocationSize = req.size;
      u_mai.memoryTypeIndex = FindMemoryType(ctx, req.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      ubo_mem_.push_back(vk::raii::DeviceMemory{device, u_mai});
      ubo_buf_.back().bindMemory(*ubo_mem_.back(), 0);
      ubo_map_[i] = ubo_mem_.back().mapMemory(0, sizeof(CameraUBO));

      for (uint32_t m = 0; m < 2; ++m)
      {
        vk::DescriptorBufferInfo bi{*ubo_buf_[i], 0, sizeof(CameraUBO)};
        vk::DescriptorImageInfo ii{*common_sampler_, *textures_[m].view, vk::ImageLayout::eShaderReadOnlyOptimal};

        vk::WriteDescriptorSet w_ubo{};
        w_ubo.dstSet = *ds_[i * 2 + m];
        w_ubo.dstBinding = 0;
        w_ubo.descriptorCount = 1;
        w_ubo.descriptorType = vk::DescriptorType::eUniformBuffer;
        w_ubo.pBufferInfo = &bi;

        vk::WriteDescriptorSet w_smp{};
        w_smp.dstSet = *ds_[i * 2 + m];
        w_smp.dstBinding = 1;
        w_smp.descriptorCount = 1;
        w_smp.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        w_smp.pImageInfo = &ii;

        device.updateDescriptorSets({w_ubo, w_smp}, {});
      }
    }

    // 7. Pipeline (使用最稳妥的显式赋值)
    auto vert_code = ReadFile("res/vk/mesh.vert.spv");
    auto frag_code = ReadFile("res/vk/mesh.frag.spv");
    vk::ShaderModuleCreateInfo vm_ci{};
    vm_ci.codeSize = vert_code.size();
    vm_ci.pCode = reinterpret_cast<uint32_t const *>(vert_code.data());
    vk::raii::ShaderModule vm{device, vm_ci};

    vk::ShaderModuleCreateInfo fm_ci{};
    fm_ci.codeSize = frag_code.size();
    fm_ci.pCode = reinterpret_cast<uint32_t const *>(frag_code.data());
    vk::raii::ShaderModule fm{device, fm_ci};

    vk::PipelineShaderStageCreateInfo ss[2]{};
    ss[0].stage = vk::ShaderStageFlagBits::eVertex;
    ss[0].module = *vm;
    ss[0].pName = "main";
    ss[1].stage = vk::ShaderStageFlagBits::eFragment;
    ss[1].module = *fm;
    ss[1].pName = "main";

    //   auto bdesc = VertexBindingDescription();
    //   auto adesc = VertexAttributeDescriptions();
    //   vk::PipelineVertexInputStateCreateInfo vi{};
    //   vi.vertexBindingDescriptionCount = 1;
    //   vi.pVertexBindingDescriptions = &bdesc;
    //   vi.vertexAttributeDescriptionCount = (uint32_t)adesc.size();
    //   vi.pVertexAttributeDescriptions = adesc.data();
    // 获取支持 Instancing 的描述 (对应你改好的 Vertex.hpp)
    // 1. 获取支持 Instancing 的描述 (调用你改好的新函数)
    // 注意：变量名建议用复数 bdescs 和 adescs，因为它们现在是 vector
    auto bdescs = VertexBindingDescriptions();
    auto adescs = VertexAttributeDescriptions();

    // 2. 重新配置顶点输入状态
    vk::PipelineVertexInputStateCreateInfo vi{};
    vi.sType = vk::StructureType::ePipelineVertexInputStateCreateInfo;

    // 关键：现在有 2 个 Binding (Vertex + Instance)
    vi.vertexBindingDescriptionCount = static_cast<uint32_t>(bdescs.size());
    vi.pVertexBindingDescriptions = bdescs.data();

    // 关键：现在有 7 个 Attribute (Pos, Normal, UV + Matrix的4列)
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(adescs.size());
    vi.pVertexAttributeDescriptions = adescs.data();

    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineRasterizationStateCreateInfo rs{};
    rs.cullMode = vk::CullModeFlagBits::eBack;
    rs.frontFace = vk::FrontFace::eCounterClockwise;
    rs.lineWidth = 1.0f;
    rs.polygonMode = vk::PolygonMode::eFill;

    vk::PipelineDepthStencilStateCreateInfo dss{};
    dss.depthTestEnable = 1;
    dss.depthWriteEnable = 1;
    dss.depthCompareOp = vk::CompareOp::eLess;

    vk::PipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo cb{};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    vk::PipelineViewportStateCreateInfo vp{};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::DynamicState dyn[] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dy{};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;

    vk::PipelineLayoutCreateInfo pl_ci{};
    vk::DescriptorSetLayout raw_dsl = *dsl_;
    pl_ci.setLayoutCount = 1;
    pl_ci.pSetLayouts = &raw_dsl;
    pipeline_layout_ = vk::raii::PipelineLayout{device, pl_ci};

    vk::Format cf = swapchain.Format();
    vk::PipelineRenderingCreateInfo rci{};
    rci.colorAttachmentCount = 1;
    rci.pColorAttachmentFormats = &cf;
    rci.depthAttachmentFormat = targets.shared_depth.format;

    vk::GraphicsPipelineCreateInfo gp{};
    gp.pNext = &rci;
    gp.stageCount = 2;
    gp.pStages = ss;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &dss;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dy;
    gp.layout = *pipeline_layout_;
    pipeline_ = vk::raii::Pipeline{device, nullptr, gp};

    return true;
  }

  void MeshPass::Destroy(VkContext &ctx)
  {
    // 必须先等待 GPU 闲置
    ctx.Device().waitIdle();
    instance_buf_ = nullptr;
    instance_mem_ = nullptr;

    // 1. 释放管线相关
    pipeline_ = nullptr;
    pipeline_layout_ = nullptr;

    // 2. 释放 UBO (先解映射，再清空容器)
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

    // 3. 释放描述符
    ds_.clear();
    dp_ = nullptr;
    dsl_ = nullptr;

    // 4. 释放模型缓冲
    ib_ = nullptr;
    ib_mem_ = nullptr;
    vb_ = nullptr;
    vb_mem_ = nullptr;

    // 5. 释放贴图资源数组
    tex_views_.clear();
    tex_imgs_.clear();
    tex_mems_.clear();
    texture_sampler_ = nullptr;

    // 6. 清理逻辑数据
    sub_meshes_.clear();
    IRenderPass::Destroy(ctx);
  }

  void MeshPass::OnSwapchainRecreated(VkContext &ctx, VkSwapchain const &swapchain, RenderTargets &)
  {
    (void)ctx;
    (void)swapchain;
  }

  void MeshPass::Record(FrameContext &frame, RenderTargets &targets)
  {
    if (!frame.cmd || !frame.globals)
      return;
    if (!targets.shared_depth.Valid())
      return;

    auto &cmd = *frame.cmd;
    uint32_t img = frame.image_index;

    // 1. 更新当前帧的 UBO 数据
    CameraUBO ubo{};
    ubo.view = frame.globals->view;
    ubo.proj = frame.globals->proj;
    ubo.model = model_matrix_;
    ubo.camera_pos = glm::vec4(frame.globals->camera_pos, 1.0f);

    // 确保索引在范围内再拷贝
    if (img < ubo_map_.size() && ubo_map_[img])
    {
      std::memcpy(ubo_map_[img], &ubo, sizeof(ubo));
    }

    // 2. 设置清理值与渲染附件
    vk::ClearValue clear_color{};
    clear_color.color = vk::ClearColorValue(std::array<float, 4>{{0.05f, 0.06f, 0.08f, 1.0f}});

    vk::ClearValue clear_depth{};
    clear_depth.depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderingAttachmentInfo color_att{};
    color_att.imageView = frame.swapchain_image_view;
    color_att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att.loadOp = vk::AttachmentLoadOp::eClear;
    color_att.storeOp = vk::AttachmentStoreOp::eStore;
    color_att.clearValue = clear_color;

    vk::RenderingAttachmentInfo depth_att{};
    depth_att.imageView = targets.shared_depth.view;
    depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depth_att.loadOp = vk::AttachmentLoadOp::eClear;
    depth_att.storeOp = vk::AttachmentStoreOp::eStore;
    depth_att.clearValue = clear_depth;

    vk::RenderingInfo ri{};
    ri.renderArea.offset = vk::Offset2D{0, 0};
    ri.renderArea.extent = frame.swapchain_extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &color_att;
    ri.pDepthAttachment = &depth_att;

    // 4. 开始渲染
    cmd.beginRendering(ri);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);

    vk::Viewport vp{0.0f, 0.0f, (float)frame.swapchain_extent.width, (float)frame.swapchain_extent.height, 0.0f, 1.0f};
    cmd.setViewport(0, vp);

    vk::Rect2D sc{{0, 0}, frame.swapchain_extent};
    cmd.setScissor(0, sc);

    // cmd.bindVertexBuffers(0, *vb_, {0});
    std::vector<vk::Buffer> vertex_buffers = {*vb_, *instance_buf_};
    std::vector<vk::DeviceSize> offsets = {0, 0};
    cmd.bindVertexBuffers(0, vertex_buffers, offsets);

    cmd.bindIndexBuffer(*ib_, 0, vk::IndexType::eUint32);

    // --- 核心：分子网格（树干、树叶）绘制 ---
    for (const auto &sm : sub_meshes_)
    {
      // 计算当前帧对应的描述符集索引 (img * 2 + 材质索引)
      uint32_t ds_idx = img * 2 + sm.textureIndex;

      if (ds_idx < ds_.size())
      {
        vk::DescriptorSet set = *ds_[ds_idx];
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout_, 0, {set}, {});
      }

      // 绘制子网格，instanceCount 目前固定为 1
      // cmd.drawIndexed(sm.indexCount, 1, sm.firstIndex, 0, 0);

      cmd.drawIndexed(sm.indexCount, instance_count_, sm.firstIndex, 0, 0);
    }

    cmd.endRendering();

    // 5. 转换回可显示布局
    TransitionImage(cmd, frame.swapchain_image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                    vk::ImageAspectFlagBits::eColor, vk::AccessFlagBits2::eColorAttachmentWrite, {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);
  }

} // namespace vkfw
