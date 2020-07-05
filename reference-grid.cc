#include "reference-grid.h"
#include "vulkan-core.h"


void ReferenceGrid::Register(
    space::core::VkAppContext *context,
    vk::UniquePipelineLayout *pipeline_layout,
    vk::UniqueRenderPass *render_pass,
    vk::UniquePipelineCache *pipeline_cache) {

  pipeline_ = space::core::GraphicsPipelineBuilder(&context->device, pipeline_layout, render_pass)
    .Create(pipeline_cache);

 
}

void ReferenceGrid::Draw(const vk::UniqueCommandBuffer *command_buffer) {
  (*command_buffer)->bindPipeline(
    vk::PipelineBindPoint::eGraphics, pipeline_.get());
}

