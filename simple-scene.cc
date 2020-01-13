#include "simple-scene.h"


Simple3DScene::Simple3DScene(const vk::core::VkAppContext *context)
  : vk_ctx_(context){
  // For multi threaded applications, we should create a command pool
  // for each thread. For this example, we just need one as we go
  // with async single core app.
  const uint32_t graphics_queue_family_index = vk_ctx.graphics_queue_family_index;
  const uint32_t present_queue_family_index = vk_ctx.present_queue_family_index;
  rendering_ctx_.command_pool = vk::core::CreateCommandPool(
    vk_ctx.device, graphics_queue_family_index);

  rendering_ctx_.graphics_queue =
    vk_ctx.device->getQueue(graphics_queue_family_index, 0);
  rendering_ctx_.present_queue =
    vk_ctx.device->getQueue(present_queue_family_index, 0);

}
