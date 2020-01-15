#ifndef __SIMPLE_SCENE_H_
#define __SIMPLE_SCENE_H_

#include <vulkan/vulkan.hpp>

#include "vulkan-core.h"



struct Vertex {
  float x, y, z, w;   // Position
};


struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint16_t> indexes;
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


  vk::UniqueSemaphore image_acquired_;
  vk::UniqueFence fence_;
};

#endif // __SIMPLE_SCENE_H_
