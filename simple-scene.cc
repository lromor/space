#include <unistd.h>
#include <iostream>
#include <optional>
#include <X11/Xlib.h>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <numeric>

#include "vulkan-core.h"
#include "simple-scene.h"

#include "shaders/simple.frag.h"
#include "shaders/simple.vert.h"

#define FENCE_TIMEOUT 100000000

static vk::UniqueDescriptorPool createDescriptorPool(
  vk::UniqueDevice &device, std::vector<vk::DescriptorPoolSize> const& poolSizes) {
  assert(!poolSizes.empty());
  uint32_t maxSets =
    std::accumulate(
      poolSizes.begin(), poolSizes.end(), 0,
      [](uint32_t sum, vk::DescriptorPoolSize const& dps) {
        return sum + dps.descriptorCount;
      });
  assert(0 < maxSets);

  vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo(
    vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, maxSets,
    poolSizes.size(), poolSizes.data());
  return device->createDescriptorPoolUnique(descriptorPoolCreateInfo);
}

std::vector<vk::UniqueFramebuffer> CreateFramebuffers(
  vk::UniqueDevice &device, vk::UniqueRenderPass &renderPass,
  std::vector<vk::UniqueImageView> const& imageViews,
  vk::UniqueImageView const& depthImageView, vk::Extent2D const& extent) {
  vk::ImageView attachments[2];
  attachments[1] = depthImageView.get();

  vk::FramebufferCreateInfo framebufferCreateInfo(
    vk::FramebufferCreateFlags(), *renderPass, depthImageView ? 2
    : 1, attachments, extent.width, extent.height, 1);
  std::vector<vk::UniqueFramebuffer> framebuffers;
  framebuffers.reserve(imageViews.size());
  for (auto const& view : imageViews) {
    attachments[0] = view.get();
    framebuffers.push_back(device->createFramebufferUnique(framebufferCreateInfo));
  }
  return framebuffers;
}

static vk::UniqueRenderPass CreateRenderPass(
  vk::UniqueDevice &device, vk::Format colorFormat, vk::Format depthFormat,
  vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear,
  vk::ImageLayout colorFinalLayout = vk::ImageLayout::ePresentSrcKHR) {
  std::vector<vk::AttachmentDescription> attachmentDescriptions;
  assert(colorFormat != vk::Format::eUndefined);
  attachmentDescriptions.push_back(
    vk::AttachmentDescription(
      vk::AttachmentDescriptionFlags(),
      colorFormat, vk::SampleCountFlagBits::e1, loadOp,
      vk::AttachmentStoreOp::eStore,
      vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare,
      vk::ImageLayout::eUndefined, colorFinalLayout));

  if (depthFormat != vk::Format::eUndefined) {
    attachmentDescriptions.push_back(
      vk::AttachmentDescription(
        vk::AttachmentDescriptionFlags(), depthFormat, vk::SampleCountFlagBits::e1,
        loadOp, vk::AttachmentStoreOp::eDontCare,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal));
  }
  vk::AttachmentReference colorAttachment(
    0, vk::ImageLayout::eColorAttachmentOptimal);
  vk::AttachmentReference depthAttachment(
    1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
  vk::SubpassDescription subpassDescription(
    vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 0,
    nullptr, 1, &colorAttachment, nullptr,
    (depthFormat != vk::Format::eUndefined) ? &depthAttachment : nullptr);
  return device->createRenderPassUnique(
    vk::RenderPassCreateInfo(
      vk::RenderPassCreateFlags(), static_cast<uint32_t>(attachmentDescriptions.size()),
      attachmentDescriptions.data(), 1, &subpassDescription));
}

static std::optional<vk::SurfaceFormatKHR> PickSurfaceFormat(
  std::vector<vk::SurfaceFormatKHR> const& formats) {
  assert(!formats.empty());
  vk::SurfaceFormatKHR picked_format = formats[0];
  if (formats.size() == 1) {
    if (formats[0].format == vk::Format::eUndefined) {
      picked_format.format = vk::Format::eB8G8R8A8Unorm;
      picked_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    }
  } else {
    // request several formats, the first found will be used
    vk::Format requested_formats[]  = {
      vk::Format::eB8G8R8A8Unorm,
      vk::Format::eR8G8B8A8Unorm,
      vk::Format::eB8G8R8Unorm,
      vk::Format::eR8G8B8Unorm
    };
    vk::ColorSpaceKHR requestedColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    for (size_t i = 0; i <
           sizeof(requested_formats) / sizeof(requested_formats[0]); i++) {
      vk::Format requestedFormat = requested_formats[i];
      auto it = std::find_if(formats.begin(),
                             formats.end(),
                             [requestedFormat, requestedColorSpace](auto const& f) {
                               return (f.format == requestedFormat)
                                 && (f.colorSpace == requestedColorSpace);
                             });
      if (it != formats.end()) {
        picked_format = *it;
        break;
      }
    }
  }
  assert(picked_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
  return picked_format;
}

static glm::mat4x4 CreateModelViewProjectionClipMatrix(vk::Extent2D const& extent) {
  float fov = glm::radians(45.0f);
  if (extent.width > extent.height) {
    fov *= static_cast<float>(extent.height) / static_cast<float>(extent.width);
  }

  glm::mat4x4 model = glm::mat4x4(1.0f);
  glm::mat4x4 view =
    glm::lookAt(glm::vec3(-5.0f, 3.0f, -10.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f));
  glm::mat4x4 projection
    = glm::perspective(fov, 1.0f, 0.1f, 100.0f);
  glm::mat4x4 clip =
    glm::mat4x4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);   // vulkan clip space has inverted y and half z !
  return clip * projection * view * model;
}

vk::UniquePipeline CreateGraphicsPipeline(
  vk::UniqueDevice const& device, vk::UniquePipelineCache const& pipelineCache,
  std::pair<vk::ShaderModule, vk::SpecializationInfo const*> const& vertexShaderData,
  std::pair<vk::ShaderModule, vk::SpecializationInfo const*> const& fragmentShaderData,
  uint32_t vertexStride,
  std::vector<std::pair<vk::Format, uint32_t>> const& vertexInputAttributeFormatOffset,
  vk::FrontFace frontFace, bool depthBuffered,
  vk::UniquePipelineLayout const& pipelineLayout,
  vk::UniqueRenderPass const& renderPass) {
  vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[2] = {
    vk::PipelineShaderStageCreateInfo(
      vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex,
      vertexShaderData.first, "main", vertexShaderData.second),
    vk::PipelineShaderStageCreateInfo(
      vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment,
      fragmentShaderData.first, "main", fragmentShaderData.second)
  };

  std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
  vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
  if (0 < vertexStride) {
    vk::VertexInputBindingDescription vertexInputBindingDescription(0, vertexStride);
    vertexInputAttributeDescriptions.reserve(vertexInputAttributeFormatOffset.size());
    for (uint32_t i=0 ; i<vertexInputAttributeFormatOffset.size() ; i++) {
      vertexInputAttributeDescriptions.push_back(
        vk::VertexInputAttributeDescription(
          i, 0, vertexInputAttributeFormatOffset[i].first,
          vertexInputAttributeFormatOffset[i].second));
    }
    pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions =
      &vertexInputBindingDescription;
    pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount =
      vertexInputAttributeDescriptions.size();
    pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions =
      vertexInputAttributeDescriptions.data();
  }

  vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
    vk::PipelineInputAssemblyStateCreateFlags(), vk::PrimitiveTopology::eTriangleList);

  vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(
    vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

  vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
    vk::PipelineRasterizationStateCreateFlags(), false, false,
    vk::PolygonMode::eLine, vk::CullModeFlagBits::eBack,
    frontFace, false, 0.0f, 0.0f, 0.0f, 1.0f);

  vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo(
    {}, vk::SampleCountFlagBits::e1);

  vk::StencilOpState stencilOpState(
    vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep,
    vk::CompareOp::eAlways);
  vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
    vk::PipelineDepthStencilStateCreateFlags(), depthBuffered, depthBuffered,
    vk::CompareOp::eLessOrEqual, false, false, stencilOpState, stencilOpState);

  vk::ColorComponentFlags colorComponentFlags(
    vk::ColorComponentFlagBits::eR
    | vk::ColorComponentFlagBits::eG
    | vk::ColorComponentFlagBits::eB
    | vk::ColorComponentFlagBits::eA);

  vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
    false, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
    vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, colorComponentFlags);
  vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
    vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
    1, &pipelineColorBlendAttachmentState, { { 1.0f, 1.0f, 1.0f, 1.0f } });

  vk::DynamicState dynamicStates[2] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor };
  vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(
    vk::PipelineDynamicStateCreateFlags(), 2, dynamicStates);

  vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo(
    vk::PipelineCreateFlags(), 2, pipelineShaderStageCreateInfos,
    &pipelineVertexInputStateCreateInfo, &pipelineInputAssemblyStateCreateInfo,
    nullptr, &pipelineViewportStateCreateInfo, &pipelineRasterizationStateCreateInfo,
    &pipelineMultisampleStateCreateInfo, &pipelineDepthStencilStateCreateInfo,
    &pipelineColorBlendStateCreateInfo, &pipelineDynamicStateCreateInfo,
    pipelineLayout.get(), renderPass.get());

  return device->createGraphicsPipelineUnique(pipelineCache.get(), graphicsPipelineCreateInfo);
}

StaticWireframeScene3D::StaticWireframeScene3D(vk::core::VkAppContext *vk_ctx)
  : vk_ctx_(vk_ctx), r_ctx_(InitRenderingContext()),
    current_buffer_(0),
    draw_fence_(vk_ctx->device->createFenceUnique(vk::FenceCreateInfo())) {}

StaticWireframeScene3D::Simple3DRenderingContext StaticWireframeScene3D::InitRenderingContext() {
  vk::PhysicalDevice &physical_device = vk_ctx_->physical_device;
  vk::core::SurfaceData &surface_data = vk_ctx_->surface_data;
  vk::UniqueDevice &device = vk_ctx_->device;

  // For multi threaded applications, we should create a command pool
  // for each thread. For this example, we just need one as we go
  // with async single core app.
  const uint32_t graphics_queue_family_index = vk_ctx_->graphics_queue_family_index;
  const uint32_t present_queue_family_index = vk_ctx_->present_queue_family_index;
  vk::UniqueCommandPool command_pool =
    vk::core::CreateCommandPool(vk_ctx_->device, graphics_queue_family_index);

  vk::UniqueCommandBuffer command_buffer =
    std::move(
      device->allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo(
          command_pool.get(),
          vk::CommandBufferLevel::ePrimary, 1)).front());

  vk::Queue graphics_queue =
    device->getQueue(graphics_queue_family_index, 0);
  vk::Queue present_queue =
    device->getQueue(present_queue_family_index, 0);

  vk::core::SwapChainData swap_chain_data(
    physical_device, device, *surface_data.surface, surface_data.extent,
    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    vk::UniqueSwapchainKHR(), graphics_queue_family_index,
    present_queue_family_index);

  vk::core::DepthBufferData depth_buffer_data(
    physical_device, device, vk::Format::eD16Unorm, surface_data.extent);

  vk::core::BufferData uniform_buffer_data(
    physical_device, device, sizeof(glm::mat4x4),
    vk::BufferUsageFlagBits::eUniformBuffer);

  vk::core::CopyToDevice(
    device, uniform_buffer_data.deviceMemory,
    CreateModelViewProjectionClipMatrix(surface_data.extent));

  vk::UniqueDescriptorSetLayout descriptor_set_layout =
    vk::core::CreateDescriptorSetLayout(
      device, { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex} });
  vk::UniquePipelineLayout pipeline_layout =
    device->createPipelineLayoutUnique(
      vk::PipelineLayoutCreateInfo(
        vk::PipelineLayoutCreateFlags(), 1, &descriptor_set_layout.get()));

  vk::UniqueRenderPass render_pass =
    CreateRenderPass(
      device, PickSurfaceFormat(
        physical_device.getSurfaceFormatsKHR(
          surface_data.surface.get()))->format, depth_buffer_data.format);

  std::vector<vk::UniqueFramebuffer> framebuffers =
    CreateFramebuffers(
      device, render_pass, swap_chain_data.image_views,
      depth_buffer_data.image_view, surface_data.extent);

  vk::UniqueDescriptorPool descriptor_pool =
    createDescriptorPool(device, { {vk::DescriptorType::eUniformBuffer, 1} });
  vk::UniqueDescriptorSet descriptor_set =
    std::move(
      device->allocateDescriptorSetsUnique(
        vk::DescriptorSetAllocateInfo(*descriptor_pool, 1, &*descriptor_set_layout)).front());

  vk::core::UpdateDescriptorSets(
    device, descriptor_set,
    {{vk::DescriptorType::eUniformBuffer, uniform_buffer_data.buffer, vk::UniqueBufferView()}});

  vk::UniqueShaderModule vertex =
    device->createShaderModuleUnique(
      vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), sizeof(simple_vert), simple_vert));


  vk::UniqueShaderModule frag =
    device->createShaderModuleUnique(
      vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), sizeof(simple_frag), simple_frag));

  vk::UniquePipelineCache pipeline_cache =
    device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo());

  vk::UniquePipeline graphics_pipeline =
    CreateGraphicsPipeline(
      device, pipeline_cache, std::make_pair(
        *vertex, nullptr),
      std::make_pair(*frag, nullptr),
      sizeof(Vertex),
      { { vk::Format::eR32G32B32A32Sfloat, 0 }, { vk::Format::eR32G32B32A32Sfloat, 16 } },
      vk::FrontFace::eClockwise, true, pipeline_layout, render_pass);

  vk::PipelineStageFlags wait_destination_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

  StaticWireframeScene3D::Simple3DRenderingContext r_ctx{
    std::move(command_pool), std::move(command_buffer), graphics_queue, present_queue,
    std::move(swap_chain_data), std::move(depth_buffer_data), std::move(uniform_buffer_data),
    std::move(descriptor_set_layout), std::move(pipeline_layout),
    std::move(render_pass), std::move(framebuffers), std::move(descriptor_pool),
    std::move(descriptor_set), std::move(pipeline_cache), std::move(graphics_pipeline),
    wait_destination_stage_mask};

  return r_ctx;
}

void StaticWireframeScene3D::AddMesh(const Mesh &mesh) {

  vk::PhysicalDevice &physical_device = vk_ctx_->physical_device;
  vk::UniqueDevice &device = vk_ctx_->device;

  const unsigned int nvertices = mesh.vertices.size();

  // Create the index and vertex buffer
  vertex_buffer_data_.push_back(
    vk::core::BufferData(
      physical_device, device, nvertices * sizeof(Vertex),
      vk::BufferUsageFlagBits::eVertexBuffer));

  const auto &vertex_data = vertex_buffer_data_.back();
  // Submit them to the device
  vk::core::CopyToDevice(device, vertex_data.deviceMemory, mesh.vertices.data(), nvertices);

  const unsigned int nindexes = mesh.indexes.size();
  index_buffer_data_.push_back(
    vk::core::BufferData(
      physical_device, device, nindexes * sizeof(Vertex),
      vk::BufferUsageFlagBits::eIndexBuffer));

  const auto &index_data = index_buffer_data_.back();

  // Submit them to the device
  vk::core::CopyToDevice(device, index_data.deviceMemory, mesh.indexes.data(), nvertices);
  meshes_.push_back(mesh);
}


void StaticWireframeScene3D::SubmitRendering() {
  const vk::UniqueDevice &device = vk_ctx_->device;
  const vk::core::SwapChainData &swap_chain_data = r_ctx_.swap_chain_data;
  const vk::Queue &graphics_queue = r_ctx_.graphics_queue;
  const vk::UniqueCommandBuffer &command_buffer = r_ctx_.command_buffer;
  const vk::UniqueRenderPass &render_pass = r_ctx_.render_pass;
  const std::vector<vk::UniqueFramebuffer> &framebuffers = r_ctx_.framebuffers;
  const vk::UniquePipeline &graphics_pipeline = r_ctx_.graphics_pipeline;
  const vk::UniquePipelineLayout &pipeline_layout = r_ctx_.pipeline_layout;
  const vk::UniqueDescriptorSet &descriptor_set = r_ctx_.descriptor_set;
  const vk::core::SurfaceData &surface_data = vk_ctx_->surface_data;

  // Get the index of the next available swapchain image:
  vk::UniqueSemaphore imageAcquiredSemaphore = device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
  vk::ResultValue<uint32_t> res =
    device->acquireNextImageKHR(
      swap_chain_data.swap_chain.get(), FENCE_TIMEOUT,
      imageAcquiredSemaphore.get(), nullptr);
  assert(res.result == vk::Result::eSuccess);
  assert(res.value < r_ctx_.framebuffers.size());
  current_buffer_ = res.value;
  command_buffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

  vk::ClearValue clear_values[2];
  clear_values[0].color =
    vk::ClearColorValue(std::array<float, 4>({ 0.2f, 0.2f, 0.2f, 0.2f }));
  clear_values[1].depthStencil =
    vk::ClearDepthStencilValue(1.0f, 0);
  vk::RenderPassBeginInfo renderPassBeginInfo(
    render_pass.get(), framebuffers[current_buffer_].get(),
    vk::Rect2D(vk::Offset2D(0, 0), surface_data.extent), 2, clear_values);

  command_buffer->beginRenderPass(
    renderPassBeginInfo, vk::SubpassContents::eInline);
  command_buffer->bindPipeline(
    vk::PipelineBindPoint::eGraphics, graphics_pipeline.get());
  command_buffer->bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics, pipeline_layout.get(), 0, descriptor_set.get(), nullptr);

  command_buffer->bindVertexBuffers(0, *vertex_buffer_data_.back().buffer, {0});
  command_buffer->bindIndexBuffer(*index_buffer_data_.back().buffer, 0, vk::IndexType::eUint16);

  command_buffer->setViewport(
    0, vk::Viewport(
      0.0f, 0.0f,
      static_cast<float>(surface_data.extent.width),
      static_cast<float>(surface_data.extent.height), 0.0f, 1.0f));
  command_buffer->setScissor(
    0, vk::Rect2D(vk::Offset2D(0, 0), surface_data.extent));

  command_buffer->draw(12 * 3, 1, 0, 0);
  command_buffer->endRenderPass();
  command_buffer->end();

  device->resetFences(1, &draw_fence_.get());
  vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
  vk::SubmitInfo submitInfo(1, &imageAcquiredSemaphore.get(), &waitDestinationStageMask, 1, &command_buffer.get());
  graphics_queue.submit(submitInfo, draw_fence_.get());


}

void StaticWireframeScene3D::Present() {
  vk::UniqueDevice &device = vk_ctx_->device;
  vk::core::SwapChainData &swap_chain_data = r_ctx_.swap_chain_data;
  vk::Queue &present_queue = r_ctx_.present_queue;

  while (vk::Result::eTimeout
         == device->waitForFences(draw_fence_.get(), VK_TRUE, FENCE_TIMEOUT)) { usleep(1000); }
  present_queue.presentKHR(
    vk::PresentInfoKHR(0, nullptr, 1, &swap_chain_data.swap_chain.get(), &current_buffer_));
}
