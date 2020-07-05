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
#include <cmath>

#include "vulkan-core.h"

#include "curve.h"
#include "shaders/curve.vert.h"
#include "shaders/curve.frag.h"
#include <iostream>

// Return a spiral.
static inline Curve::Point Spiral(const float t, const float a = 1.0, const float b = 1.0) {
  float x, y, z;
  x = a * cos(t);
  y = b * sin(t);
  z = b * t;
  return { x, y, z };
}

void Curve::Update(const float t) {

}

void Curve::Register(
    space::core::VkAppContext *context,
    vk::UniquePipelineLayout *pipeline_layout,
    vk::UniqueRenderPass *render_pass,
    vk::UniquePipelineCache *pipeline_cache) {

  // Instantiate the shaders
  vk::UniqueShaderModule vertex =
    context->device->createShaderModuleUnique(
      vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), sizeof(curve_vert), curve_vert));

  vk::UniqueShaderModule frag =
    context->device->createShaderModuleUnique(
      vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), sizeof(curve_frag), curve_frag));

  pipeline_ = space::core::GraphicsPipelineBuilder(&context->device, pipeline_layout, render_pass)
    .DepthBuffered(true)
    .SetPrimitiveTopology(vk::PrimitiveTopology::eLineList)
    .SetPolygoneMode(vk::PolygonMode::eLine)
    .AddVertexShader(*vertex)
    .AddFragmentShader(*frag)
    .AddVertexInputBindingDescription(0, sizeof(Curve::Point), vk::VertexInputRate::eVertex)
    .AddVertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0)
    .EnableDynamicState(vk::DynamicState::eScissor)
    .EnableDynamicState(vk::DynamicState::eViewport)
    .Create(pipeline_cache);

  double resolution = 1e-3;
  double t_init = 20.0;
  unsigned nsteps = t_init / resolution;

  for (int i = 0; i < nsteps; ++i) {
    const float ti = resolution * i;
    const Point p = Spiral(ti);
    points_.push_back(p);
  }

  vertex_buffer_data_ = std::make_unique<space::core::BufferData>(
    context->physical_device, context->device, points_.size() * sizeof(Point),
    vk::BufferUsageFlagBits::eVertexBuffer);

  // Submit them to the device
  space::core::CopyToDevice(
    context->device, vertex_buffer_data_->deviceMemory, points_.data(), points_.size());
}

void Curve::Draw(const vk::UniqueCommandBuffer *command_buffer) {
  const vk::UniqueCommandBuffer &cb  = *command_buffer;

  // Tell vulkan the next commands are associated to this pipeline.
  cb->bindPipeline(
    vk::PipelineBindPoint::eGraphics, pipeline_.get());

  // Tell vulkan which buffer contains the vertices we want to draw.
  cb->bindVertexBuffers(0, *vertex_buffer_data_->buffer, {0});
  cb->draw(points_.size(), 1, 0, 0);
}

