// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright(c) Leonardo Romor <leonardo.romor@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

#ifndef __SIMPLE_SCENE_H_
#define __SIMPLE_SCENE_H_

#include <iostream>
#include <vulkan/vulkan.hpp>

#include "vulkan-core.h"
#include "entity.h"
#include "input/gamepad.h"
#include "camera.h"

// Given an initialized vulkan context
// perform rendering of the entities.
class Scene {
public:
  typedef std::function<vk::Extent2D()> QueryExtentCallback;
  Scene(space::core::VkAppContext *context, Camera *camera, const QueryExtentCallback &fn);

  void Init();
  void AddEntity(space::Entity *entity);
  void SubmitRendering();
  void Present();

private:
  space::core::VkAppContext *const vk_ctx_;
  const QueryExtentCallback QueryExtent;

  vk::UniqueCommandPool command_pool_;
  vk::Queue graphics_queue_;
  vk::Queue present_queue_;

  vk::UniqueDescriptorSetLayout descriptor_set_layout_;

  // The pipeline layout used to describe
  // how descriptors should be used.
  vk::UniquePipelineLayout pipeline_layout_;
  vk::UniquePipelineCache pipeline_cache_;

  // Define the set of objects to be recreated
  // in case of an out-of-date swapchain.
  struct SwapChainContext {
    vk::UniqueCommandBuffer command_buffer;
    space::core::SwapChainData swap_chain_data;

    // For multisampling
    space::core::ImageData color_buffer_data;

    // Depth buffer data. Contains the resulting
    // depth pseudoimage.
    space::core::DepthBufferData depth_buffer_data;

    // Uniform buffer containing projection matrix
    // and other shared buffers.
    space::core::BufferData uniform_buffer_data;

    // We are using a single render pass
    vk::UniqueRenderPass render_pass;
    std::vector<vk::UniqueFramebuffer> framebuffers;
    vk::UniqueDescriptorPool descriptor_pool;
    vk::UniqueDescriptorSet descriptor_set;
  };

  std::unique_ptr<SwapChainContext> swap_chain_context_;

  // Creates a new swapchain and returns the old one.
  void CreateSwapChainContext();

  uint32_t current_buffer_;

  // Fence for when the rendering is done
  // and we are ready to present :)
  vk::UniqueFence draw_fence_;

  std::vector<space::Entity *> entities_;
  Camera *camera_;
};

#endif // __SIMPLE_SCENE_H_
