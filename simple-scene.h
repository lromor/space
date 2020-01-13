#ifndef __SIMPLE_SCENE_H_
#define __SIMPLE_SCENE_H_

// Given an initialized vulkan context
// perform a basic rendering of a static scene
// where n meshes can be added dynamically
// and camera with rotation/translation can be changed.
// The scene renders the 3d object in wireframe mode.
class StaticDWireframeScene3D {
public:
  StaticDWireframeScene3D(const vk::core::VkAppContext *context) {}

private:

  struct Simple3DRenderingContext {
    vk::UniqueCommandPool command_pool;
    vk::Queue graphics_queue;
    vk::Queue present_queue
  };

  Simple3DRenderingContext rendering_ctx_;
  vk::core::VkAppContext *const vk_ctx_;

  void InitializeObjects
};

#endif // __SIMPLE_SCENE_H_
