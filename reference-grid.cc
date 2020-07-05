#include "reference-grid.h"
#include "vulkan-core.h"
#include "shaders/grid.vert.h"
#include "shaders/grid.frag.h"

#include <iostream>
void ReferenceGrid::Register(
    space::core::VkAppContext *context,
    vk::UniquePipelineLayout *pipeline_layout,
    vk::UniqueRenderPass *render_pass,
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


  pipeline_ = space::core::GraphicsPipelineBuilder(&context->device, pipeline_layout, render_pass)
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

