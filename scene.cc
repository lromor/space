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

#include <unistd.h>
#include <iostream>
#include <optional>
#include <X11/Xlib.h>
#include <vulkan/vulkan.hpp>
#include <numeric>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "vulkan-core.h"
#include "scene.h"
#include "shaders/simple.frag.h"
#include "shaders/simple.vert.h"

#define FENCE_TIMEOUT 100000000

Scene::Scene(space::core::VkAppContext *vk_ctx)
  : vk_ctx_(vk_ctx), r_ctx_(InitRenderingContext()),
    current_buffer_(0),
    draw_fence_(vk_ctx->device->createFenceUnique(vk::FenceCreateInfo())),
    camera_{glm::vec3(0.0f, 0.0f, -15.0f), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)},
    projection_matrices_(UpdateProjectionMatrices()) {}

Scene::RenderingContext Scene::InitRenderingContext() {
  vk::PhysicalDevice &physical_device = vk_ctx_->physical_device;
  space::core::SurfaceData &surface_data = vk_ctx_->surface_data;
  vk::UniqueDevice &device = vk_ctx_->device;

  // For multi threaded applications, we should create a command pool
  // for each thread. For this example, we just need one as we go
  // with async single core app.
  const uint32_t graphics_queue_family_index = vk_ctx_->graphics_queue_family_index;
  const uint32_t present_queue_family_index = vk_ctx_->present_queue_family_index;
  vk::UniqueCommandPool command_pool =
    space::core::CreateCommandPool(vk_ctx_->device, graphics_queue_family_index);

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

  space::core::SwapChainData swap_chain_data(
    physical_device, device, *surface_data.surface, surface_data.extent,
    vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
    vk::UniqueSwapchainKHR(), graphics_queue_family_index,
    present_queue_family_index);

  space::core::DepthBufferData depth_buffer_data(
    physical_device, device, vk::Format::eD16Unorm, surface_data.extent);

  space::core::BufferData uniform_buffer_data(
    physical_device, device, sizeof(glm::mat4x4),
    vk::BufferUsageFlagBits::eUniformBuffer);

  vk::UniqueDescriptorSetLayout descriptor_set_layout =
    space::core::CreateDescriptorSetLayout(
      device, { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex} });
  vk::UniquePipelineLayout pipeline_layout =
    device->createPipelineLayoutUnique(
      vk::PipelineLayoutCreateInfo(
        vk::PipelineLayoutCreateFlags(), 1, &descriptor_set_layout.get()));

  vk::UniqueRenderPass render_pass =
    space::core::CreateRenderPass(
      device, space::core::PickSurfaceFormat(
        physical_device.getSurfaceFormatsKHR(
          surface_data.surface.get()))->format, depth_buffer_data.format);

  std::vector<vk::UniqueFramebuffer> framebuffers =
    space::core::CreateFramebuffers(
      device, render_pass, swap_chain_data.image_views,
      depth_buffer_data.image_view, surface_data.extent);

  vk::UniqueDescriptorPool descriptor_pool =
    space::core::CreateDescriptorPool(device, { {vk::DescriptorType::eUniformBuffer, 1} });
  vk::UniqueDescriptorSet descriptor_set =
    std::move(
      device->allocateDescriptorSetsUnique(
        vk::DescriptorSetAllocateInfo(*descriptor_pool, 1, &*descriptor_set_layout)).front());

  space::core::UpdateDescriptorSets(
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

  Scene::RenderingContext r_ctx{
    std::move(command_pool), std::move(command_buffer), graphics_queue, present_queue,
    std::move(swap_chain_data), std::move(depth_buffer_data), std::move(uniform_buffer_data),
    std::move(descriptor_set_layout), std::move(pipeline_layout),
    std::move(render_pass), std::move(framebuffers), std::move(descriptor_pool),
    std::move(descriptor_set), std::move(pipeline_cache)};

  return r_ctx;
}

void Scene::AddEntity(space::Entity *entity) {
  // Initialize entity
  entity->Register(vk_ctx_, &r_ctx_.pipeline_layout, &r_ctx_.render_pass,
                   &r_ctx_.pipeline_cache);
  entities_.push_back(entity);
}

void Scene::SubmitRendering() {
  const vk::UniqueDevice &device = vk_ctx_->device;
  const space::core::SwapChainData &swap_chain_data = r_ctx_.swap_chain_data;
  const vk::Queue &graphics_queue = r_ctx_.graphics_queue;
  const vk::UniqueCommandBuffer &command_buffer = r_ctx_.command_buffer;
  const vk::UniqueRenderPass &render_pass = r_ctx_.render_pass;
  const std::vector<vk::UniqueFramebuffer> &framebuffers = r_ctx_.framebuffers;
  const vk::UniquePipelineLayout &pipeline_layout = r_ctx_.pipeline_layout;
  const vk::UniqueDescriptorSet &descriptor_set = r_ctx_.descriptor_set;
  const space::core::SurfaceData &surface_data = vk_ctx_->surface_data;
  const space::core::BufferData &uniform_buffer_data = r_ctx_.uniform_buffer_data;

  // Update the projection matrices with the current values of camera, model, fov, etc..
  projection_matrices_ = UpdateProjectionMatrices();
 
  // Update uniform buffer
  space::core::CopyToDevice(
    device, uniform_buffer_data.deviceMemory,  projection_matrices_.clip
    * projection_matrices_.projection * projection_matrices_.view * projection_matrices_.model);

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

   command_buffer->bindDescriptorSets(
     vk::PipelineBindPoint::eGraphics, pipeline_layout.get(), 0, descriptor_set.get(), nullptr);

   command_buffer->setViewport(
     0, vk::Viewport(
       0.0f, 0.0f,
       static_cast<float>(surface_data.extent.width),
       static_cast<float>(surface_data.extent.height), 0.0f, 1.0f));
   command_buffer->setScissor(
     0, vk::Rect2D(vk::Offset2D(0, 0), surface_data.extent));

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
  space::core::SwapChainData &swap_chain_data = r_ctx_.swap_chain_data;
  vk::Queue &present_queue = r_ctx_.present_queue;

  while (vk::Result::eTimeout
         == device->waitForFences(draw_fence_.get(), VK_TRUE, FENCE_TIMEOUT)) { usleep(1000); }
  present_queue.presentKHR(
    vk::PresentInfoKHR(0, nullptr, 1, &swap_chain_data.swap_chain.get(), &current_buffer_));
}


struct Scene::Projection Scene::UpdateProjectionMatrices() {

  vk::Extent2D &extent = vk_ctx_->surface_data.extent;

  float fov = glm::radians(60.0f);
  const auto aspect_ratio =
    static_cast<float>(extent.width) / static_cast<float>(extent.height);

  glm::mat4x4 model = glm::mat4x4(1.0f);
  glm::mat4x4 view = glm::lookAt(camera_.eye, camera_.center, camera_.up);
  glm::mat4x4 projection = glm::perspective(fov, aspect_ratio, 0.1f, 100.0f);
  glm::mat4x4 clip =
    glm::mat4x4(1.0f,  0.0f, 0.0f, 0.0f,
                0.0f, -1.0f, 0.0f, 0.0f,
                0.0f,  0.0f, 0.5f, 0.0f,
                0.0f,  0.0f, 0.5f, 1.0f);   // vulkan clip space has inverted y and half z !
  return Projection{ fov, model, view, projection, clip };
}

void Scene::Input(CameraControls &input) {
  // camera basis
  glm::vec3 nx = glm::normalize(camera_.center - camera_.eye); // front
  glm::vec3 nz = glm::normalize(glm::cross(-nx, glm::vec3(0.0f, 1.0f, 0.0f))); // side
  glm::vec3 ny = glm::normalize(glm::cross(nx, nz)); // up
 
  // We want to keep fixed the distance between center and eye.
  auto t = input.dy * 0.01f * nx - input.dx * 0.005f * nz;
  camera_.center += t;
  camera_.eye += t;
  camera_.up = ny;

  // Rotate w.r.t ny camera center.
  auto transform = glm::rotate(glm::mat4x4(1.0f), input.dphi * 0.001f, -ny);
  camera_.center = transform * (glm::vec4(camera_.center - camera_.eye, 1.0));
  camera_.center += camera_.eye;

  // Rotate w.r.t nz camera center.
  auto transform2 = glm::rotate(glm::mat4x4(1.0f), input.dtheta * 0.001f, -nz);
  camera_.center = transform2 * (glm::vec4(camera_.center - camera_.eye, 1.0));
  camera_.center += camera_.eye;
}
