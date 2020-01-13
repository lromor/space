#ifndef __SIMPLE_SCENE_H_
#define __SIMPLE_SCENE_H_

// Given an initialized vulkan context
// perform a basic rendering of a static scene
// where n meshes can be added dynamically
// and camera with rotation/translation can be changed.
// The scene renders the 3d object in wireframe mode.
class StaticWireframeScene3D {
public:
  StaticWireframeScene3D(vk::core::VkAppContext *context);

  //void AddMesh();?
  //SetViewpoint?
private:

  vk::core::VkAppContext *const vk_ctx_;

  // "Simple"
  struct Simple3DRenderingContext {
    vk::UniqueCommandPool command_pool;
    vk::Queue graphics_queue;
    vk::Queue present_queue;
    vk::UniqueCommandBuffer command_buffer;
    vk::core::SwapChainData swap_chain_data;
    vk::core::DepthBufferData depth_buffer_data;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniqueRenderPass render_pass;
    vk::UniqueDescriptorPool unique_descriptor_pool;
    vk::UniqueDescriptorSet unique_descriptor_set;
    vk::UniquePipelineCache pipeline_cache;
    vk::UniquePipeline graphics_pipeline;
    vk::PipelineStageFlags wait_destination_stage_mask;
    std::vector<vk::UniqueFramebuffer> framebuffers;

    // Buffers
    DepthBufferData depth_buffer_data;
    BufferData uniform_buffer_data;
  };

  const Simple3DRenderingContext &InitRenderingContext();
  Simple3DRenderingContext r_ctx_;

  vk::UniqueSemaphore image_acquired;
  vk::UniqueFence fence;

};

#endif // __SIMPLE_SCENE_H_
