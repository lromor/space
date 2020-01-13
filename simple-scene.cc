#include <iostream>
#include <optional>
#include <X11/Xlib.h>
#include <vulkan/vulkan.hpp>

#include "vulkan-core.h"
#include "simple-scene.h"


StaticWireframeScene3D::StaticWireframeScene3D(vk::core::VkAppContext *vk_ctx)
  : vk_ctx_(vk_ctx), r_ctx_(std::move(InitRenderingContext())) {}

const Simple3DRenderingContext &StaticWireframeScene3D::InitRenderingContext() {
  vk::PhysicalDevice &physical_device = vk_ctx_->physical_device;
  vk::core::SurfaceData &surface_data = vk_ctx_->surface_data;
  vk::UniqueDevice &device = vk_ctx_->device;

  // For multi threaded applications, we should create a command pool
  // for each thread. For this example, we just need one as we go
  // with async single core app.
  const uint32_t graphics_queue_family_index = vk_ctx->graphics_queue_family_index;
  const uint32_t present_queue_family_index = vk_ctx->present_queue_family_index;
  vk::UniqueCommandPool command_pool =
    vk::core::CreateCommandPool(vk_ctx.device, graphics_queue_family_index);

  vk::UniqueCommandBuffer command_buffer =
    std::move(
      device->allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo(
          r_ctx.command_pool.get(),
          vk::CommandBufferLevel::ePrimary, 1)).front());

  vk::Queue graphics_queue =
    device->getQueue(graphics_queue_family_index, 0);
  vk::Queue present_queue =
    device->getQueue(present_queue_family_index, 0);

  SwapChainData swap_chain_data(
    physical_device, device, *surface_data.surface, surface_data.extent,
    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    vk::UniqueSwapchainKHR(), graphics_queue_family_index,
    present_queue_family_index);

  DepthBufferData depth_buffer_data(
    physical_device, device, vk::Format::eD16Unorm, surface_data.extent);

  BufferData uniform_buffer_data(
    physical_device, device, sizeof(glm::mat4x4),
    vk::BufferUsageFlagBits::eUniformBuffer);
  vk::core::CopyToDevice(
    device, uniformBufferData.deviceMemory,
    createModelViewProjectionClipMatrix(surface_data.extent));

  vk::UniqueDescriptorSetLayout descriptorSetLayout =
    createDescriptorSetLayout(
      device, { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex} });
  vk::UniquePipelineLayout pipelineLayout =
    device->createPipelineLayoutUnique(
      vk::PipelineLayoutCreateInfo(
        vk::PipelineLayoutCreateFlags(), 1, &descriptorSetLayout.get()));

  vk::UniqueRenderPass renderPass =
    createRenderPass(
      device, PickSurfaceFormat(
        physical_device.getSurfaceFormatsKHR(
          surface_data.surface.get()))->format, depthBufferData.format);

  std::vector<vk::UniqueFramebuffer> framebuffers
    = CreateFramebuffers(
      device, renderPass, swap_chain_data.image_views,
      depthBufferData.image_view, surface_data.extent);

  vk::UniqueDescriptorPool descriptor_pool =
    createDescriptorPool(device, { {vk::DescriptorType::eUniformBuffer, 1} });
  vk::UniqueDescriptorSet descriptor_set =
    std::move(
      device->allocateDescriptorSetsUnique(
        vk::DescriptorSetAllocateInfo(*descriptorPool, 1, &*descriptorSetLayout)).front());

  vk::core::UpdateDescriptorSets(
    device, descriptor_set,
    {{vk::DescriptorType::eUniformBuffer, uniformBufferData.buffer, vk::UniqueBufferView()}}, {});

  vk::UniquePipelineCache pipelineCache =
    device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo());
  vk::UniquePipeline graphicsPipeline =
    createGraphicsPipeline(
      device, pipelineCache, std::make_pair(
        *vertexShaderModule, nullptr),
      std::make_pair(*fragmentShaderModule, nullptr),
      sizeof(coloredCubeData[0]), { { vk::Format::eR32G32B32A32Sfloat, 0 }, { vk::Format::eR32G32B32A32Sfloat, 16 } },
      vk::FrontFace::eClockwise, true, pipelineLayout, renderPass);


}
