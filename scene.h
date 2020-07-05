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
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include "vulkan-core.h"
#include "entity.h"
#include "gamepad.h"

struct CameraControls {
  CameraControls() = default;
  CameraControls operator+(const CameraControls &b) const {
    return CameraControls{
      dx + b.dx, dy + b.dy, dz + b.dz,
        dphi + b.dphi, dtheta + b.dtheta};
  }
  float dx, dy, dz;
  float dphi, dtheta;
};

// Given an initialized vulkan context
// perform rendering of the entities.
class Scene {
public:
  Scene(space::core::VkAppContext *context);

  void AddEntity(space::Entity *entity);
  void Input(CameraControls &input);
  void SubmitRendering();
  void Present();

private:
  space::core::VkAppContext *const vk_ctx_;

  struct RenderingContext {
    vk::UniqueCommandPool command_pool;
    vk::UniqueCommandBuffer command_buffer;
    vk::Queue graphics_queue;
    vk::Queue present_queue;

    space::core::SwapChainData swap_chain_data;

    // Depth buffer data. Contains the resulting
    // depth pseudoimage.
    space::core::DepthBufferData depth_buffer_data;

    // Uniform buffer containing projection matrix
    // and other shared buffers.
    space::core::BufferData uniform_buffer_data;

    vk::UniqueDescriptorSetLayout descriptor_set_layout;

    // The pipeline layout used to describe
    // how descriptors should be used.
    vk::UniquePipelineLayout pipeline_layout;

    // We are using a single render pass
    vk::UniqueRenderPass render_pass;
    std::vector<vk::UniqueFramebuffer> framebuffers;
    vk::UniqueDescriptorPool descriptor_pool;
    vk::UniqueDescriptorSet descriptor_set;
    vk::UniquePipelineCache pipeline_cache;
  };

  // Initialize the rendering context
  struct RenderingContext InitRenderingContext();
  RenderingContext r_ctx_;

  uint32_t current_buffer_;

  // Fence for when the rendering is done
  // and we are ready to present :)
  vk::UniqueFence draw_fence_;

  struct Camera {
    glm::vec3 eye; // Camera position.
    glm::vec3 center; // Looking point.
    glm::vec3 up; // Camera vertical axis.
  };

  struct Camera camera_;

  struct Projection {
    float fov;
    glm::mat4x4 model;
    glm::mat4x4 view;
    glm::mat4x4 projection;
    glm::mat4x4 clip;
  };

  struct Projection projection_matrices_;
  struct Projection UpdateProjectionMatrices();


  std::vector<space::Entity *> entities_;
};

#endif // __SIMPLE_SCENE_H_
