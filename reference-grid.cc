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
#include "reference-grid.h"
#include "vulkan-core.h"
#include "shaders/grid.vert.h"
#include "shaders/grid.frag.h"

#include <iostream>
void ReferenceGrid::Register(
    space::core::VkAppContext *context,
    vk::UniquePipelineLayout *pipeline_layout,
    vk::UniqueRenderPass *render_pass,
    vk::SampleCountFlagBits nsamples,
    vk::UniquePipelineCache *pipeline_cache) {


  // Instantiate the shaders
  vk::UniqueShaderModule vertex =
    context->device->createShaderModuleUnique(
      vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), sizeof(grid_vert), grid_vert));

  vk::UniqueShaderModule frag =
    context->device->createShaderModuleUnique(
      vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), sizeof(grid_frag), grid_frag));


  pipeline_ = space::core::GraphicsPipelineBuilder(
    &context->device, pipeline_layout, render_pass, nsamples)
    .DepthBuffered(true)
    .SetPrimitiveTopology(vk::PrimitiveTopology::eTriangleList)
    .SetPolygoneMode(vk::PolygonMode::eFill)
    .AddVertexShader(*vertex)
    .AddFragmentShader(*frag)
    .EnableDynamicState(vk::DynamicState::eScissor)
    .EnableDynamicState(vk::DynamicState::eViewport)
    .Create(pipeline_cache);
}

void ReferenceGrid::Draw(const vk::UniqueCommandBuffer *command_buffer) {
  (*command_buffer)->bindPipeline(
    vk::PipelineBindPoint::eGraphics, pipeline_.get());

  (*command_buffer)->draw(6, 1, 0, 0);
}

