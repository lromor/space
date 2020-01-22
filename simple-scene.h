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
#include "gamepad.h"


struct Vertex {
  float x, y, z, w;   // Position
};


struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint16_t> indexes;
};

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
// perform a basic rendering of a static scene
// where n meshes can be added dynamically
// and camera with rotation/translation can be changed.
// The scene renders the 3d object in wireframe mode.
class StaticWireframeScene3D {
public:
  StaticWireframeScene3D(vk::core::VkAppContext *context);

  void AddMesh(const Mesh &mesh);
  void Input(CameraControls &input);
  void SubmitRendering();
  void Present();

  //SetViewpoint?
private:

  vk::core::VkAppContext *const vk_ctx_;

  // "Simple"
  struct Simple3DRenderingContext {
    vk::UniqueCommandPool command_pool;
    vk::UniqueCommandBuffer command_buffer;
    vk::Queue graphics_queue;
    vk::Queue present_queue;

    vk::core::SwapChainData swap_chain_data;

    vk::core::DepthBufferData depth_buffer_data;
    vk::core::BufferData uniform_buffer_data;


    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniqueRenderPass render_pass;
    std::vector<vk::UniqueFramebuffer> framebuffers;
    vk::UniqueDescriptorPool descriptor_pool;
    vk::UniqueDescriptorSet descriptor_set;
    vk::UniquePipelineCache pipeline_cache;
    vk::UniquePipeline graphics_pipeline;
    vk::PipelineStageFlags wait_destination_stage_mask;

  };

  struct Simple3DRenderingContext InitRenderingContext();
  Simple3DRenderingContext r_ctx_;

  std::vector<vk::core::BufferData> vertex_buffer_data_;
  std::vector<vk::core::BufferData> index_buffer_data_;
  std::vector<Mesh> meshes_;

  uint32_t current_buffer_;
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
};

#endif // __SIMPLE_SCENE_H_
