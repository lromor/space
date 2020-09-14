// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright(c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// Modifications copyright (C) 2020 Leonardo Romor <leonardo.romor@gmail.com>
//
// This file contains the basic ingredients to render a basic wireframed scene.
// This simple scene allows you to add meshes and a freely "movable" camera.
#include <glm/ext/quaternion_geometric.hpp>
#include <unistd.h>
#include <iostream>
#include <optional>
#include <X11/Xlib.h>
#include <vulkan/vulkan.hpp>
#include <numeric>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "vulkan-core.h"
#include "scene.h"

#define FENCE_TIMEOUT 100000000

Scene::Scene(space::core::VkAppContext *vk_ctx, Camera *camera, const QueryExtentCallback &fn)
  : vk_ctx_(vk_ctx),  QueryExtent(fn), current_buffer_(0),
    draw_fence_(vk_ctx->device->createFenceUnique(vk::FenceCreateInfo())),
    camera_(camera) {}

void Scene::Init() {
  vk::UniqueDevice &device = vk_ctx_->device;

  const uint32_t graphics_queue_family_index = vk_ctx_->graphics_queue_family_index;
  const uint32_t present_queue_family_index = vk_ctx_->present_queue_family_index;

  // For multi threaded applications, we should create a command pool
  // for each thread. For this example, we just need one as we go
  // with async single core app.
  command_pool_ =
    space::core::CreateCommandPool(vk_ctx_->device, graphics_queue_family_index);
  graphics_queue_ = device->getQueue(graphics_queue_family_index, 0);
  present_queue_ = device->getQueue(present_queue_family_index, 0);

  descriptor_set_layout_ =
    space::core::CreateDescriptorSetLayout(
      device, { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex} });
  pipeline_layout_ =
    device->createPipelineLayoutUnique(
      vk::PipelineLayoutCreateInfo(
        vk::PipelineLayoutCreateFlags(), 1, &descriptor_set_layout_.get()));

  pipeline_cache_ =
    device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo());

  CreateSwapChainContext();
}

void Scene::CreateSwapChainContext() {
  vk::PhysicalDevice &physical_device = vk_ctx_->physical_device;
  vk::UniqueSurfaceKHR &surface = vk_ctx_->surface;
  vk::UniqueDevice &device = vk_ctx_->device;
  const uint32_t graphics_queue_family_index = vk_ctx_->graphics_queue_family_index;
  const uint32_t present_queue_family_index = vk_ctx_->present_queue_family_index;

  const vk::Extent2D extent = QueryExtent();
  vk::UniqueCommandBuffer command_buffer =
    std::move(
      device->allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo(
          *command_pool_, vk::CommandBufferLevel::ePrimary, 1)).front());

  // Wait device to be idle before destroying everything
  if (swap_chain_context_)
    device->waitIdle();

  space::core::SwapChainData swap_chain_data(
    physical_device, device, *surface, extent,
    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    swap_chain_context_ ? std::move(swap_chain_context_->swap_chain_data.swap_chain) : vk::UniqueSwapchainKHR(),
    graphics_queue_family_index, present_queue_family_index);

  space::core::DepthBufferData depth_buffer_data(
    physical_device, device, vk::Format::eD16Unorm, swap_chain_data.extent);

  space::core::BufferData uniform_buffer_data(
    physical_device, device, sizeof(glm::mat4x4),
    vk::BufferUsageFlagBits::eUniformBuffer);

  vk::UniqueRenderPass render_pass =
    space::core::CreateRenderPass(
      device, space::core::PickSurfaceFormat(
        physical_device.getSurfaceFormatsKHR(
          *surface))->format, depth_buffer_data.format);

  std::vector<vk::UniqueFramebuffer> framebuffers =
    space::core::CreateFramebuffers(
      device, render_pass, swap_chain_data.image_views,
      depth_buffer_data.image_view, swap_chain_data.extent);

  vk::UniqueDescriptorPool descriptor_pool =
    space::core::CreateDescriptorPool(device, { {vk::DescriptorType::eUniformBuffer, 1} });
  vk::UniqueDescriptorSet descriptor_set =
    std::move(
      device->allocateDescriptorSetsUnique(
        vk::DescriptorSetAllocateInfo(*descriptor_pool, 1, &*descriptor_set_layout_)).front());

  space::core::UpdateDescriptorSets(
    device, descriptor_set,
    {{vk::DescriptorType::eUniformBuffer, uniform_buffer_data.buffer, vk::UniqueBufferView()}});

  struct SwapChainContext *swap_chain_context = new SwapChainContext{
    std::move(command_buffer), std::move(swap_chain_data), std::move(depth_buffer_data),
    std::move(uniform_buffer_data), std::move(render_pass), std::move(framebuffers),
    std::move(descriptor_pool), std::move(descriptor_set)};

  swap_chain_context_.reset(swap_chain_context);

  for (const auto entity : entities_) {
    entity->Register(vk_ctx_, &pipeline_layout_, &swap_chain_context_->render_pass,
                     &pipeline_cache_);
  }
}

void Scene::AddEntity(space::Entity *entity) {
  // Initialize entity
  entity->Register(vk_ctx_, &pipeline_layout_, &swap_chain_context_->render_pass,
                   &pipeline_cache_);
  entities_.push_back(entity);
}

void Scene::SubmitRendering() {
  const vk::UniqueDevice &device = vk_ctx_->device;
  const space::core::SwapChainData &swap_chain_data = swap_chain_context_->swap_chain_data;
  const vk::Queue &graphics_queue = graphics_queue_;
  const vk::UniqueCommandBuffer &command_buffer = swap_chain_context_->command_buffer;
  const vk::UniqueRenderPass &render_pass = swap_chain_context_->render_pass;
  const std::vector<vk::UniqueFramebuffer> &framebuffers = swap_chain_context_->framebuffers;
  const vk::UniquePipelineLayout &pipeline_layout = pipeline_layout_;
  const vk::UniqueDescriptorSet &descriptor_set = swap_chain_context_->descriptor_set;
  const space::core::BufferData &uniform_buffer_data = swap_chain_context_->uniform_buffer_data;

  vk::Extent2D extent = swap_chain_context_->swap_chain_data.extent;
  const auto aspect_ratio =
    static_cast<float>(extent.width) / static_cast<float>(extent.height);

  // Update the projection matrices with the current values of camera, model, fov, etc..
  auto projection_matrices = camera_->GetProjectionMatrices(aspect_ratio);
 
  // Update uniform buffer
  space::core::CopyToDevice(
    device, uniform_buffer_data.deviceMemory,  projection_matrices.clip
    * projection_matrices.projection * projection_matrices.view * projection_matrices.model);

  // Get the index of the next available swapchain image:
  vk::UniqueSemaphore imageAcquiredSemaphore = device->createSemaphoreUnique(vk::SemaphoreCreateInfo());

  bool out_of_date = false;
  try {
    vk::ResultValue<uint32_t> res =
      device->acquireNextImageKHR(
        swap_chain_data.swap_chain.get(), FENCE_TIMEOUT,
        imageAcquiredSemaphore.get(), nullptr);
    if (res.result == vk::Result::eSuboptimalKHR)
      out_of_date = true;
    assert(res.value < swap_chain_context_->framebuffers.size());
    current_buffer_ = res.value;
  } catch (vk::OutOfDateKHRError &) {
    out_of_date = true;
  }
  if (out_of_date) {
    // Re-create the swapchain context.
    CreateSwapChainContext();
    // Re-submit rendering
    return SubmitRendering();
  }

  command_buffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

  vk::ClearValue clear_values[2];
  clear_values[0].color =
    vk::ClearColorValue(std::array<float, 4>({ 0.9f, 0.9f, 0.9f, 1.0f }));
  clear_values[1].depthStencil =
    vk::ClearDepthStencilValue(1.0f, 0);
  vk::RenderPassBeginInfo renderPassBeginInfo(
    render_pass.get(), framebuffers[current_buffer_].get(),
    vk::Rect2D(vk::Offset2D(0, 0), swap_chain_data.extent), 2, clear_values);

  command_buffer->beginRenderPass(
    renderPassBeginInfo, vk::SubpassContents::eInline);

  command_buffer->bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics, pipeline_layout.get(), 0, descriptor_set.get(), nullptr);

  command_buffer->setViewport(
    0, vk::Viewport(
      0.0f, 0.0f,
      static_cast<float>(swap_chain_data.extent.width),
      static_cast<float>(swap_chain_data.extent.height), 0.0f, 1.0f));
  command_buffer->setScissor(
    0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_data.extent));

  for (const auto entity : entities_) {
    entity->Draw(&command_buffer);
  }

  command_buffer->endRenderPass();
  command_buffer->end();

  device->resetFences(1, &draw_fence_.get());
  vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
  vk::SubmitInfo submitInfo(1, &imageAcquiredSemaphore.get(), &waitDestinationStageMask, 1, &command_buffer.get());
  graphics_queue.submit(submitInfo, draw_fence_.get());
}

void Scene::Present() {
  vk::UniqueDevice &device = vk_ctx_->device;
  space::core::SwapChainData &swap_chain_data = swap_chain_context_->swap_chain_data;
  vk::Queue &present_queue = present_queue_;

  while (vk::Result::eTimeout
         == device->waitForFences(draw_fence_.get(), VK_TRUE, FENCE_TIMEOUT)) { usleep(1000); }

  try {
    auto result = present_queue.presentKHR(
      vk::PresentInfoKHR(0, nullptr, 1, &swap_chain_data.swap_chain.get(), &current_buffer_));
    assert(result == vk::Result::eSuccess);
  } catch (vk::OutOfDateKHRError &) {
    // Re-create the swapchain context.
    CreateSwapChainContext();
  }
}
