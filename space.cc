
#include <iostream>
#include <optional>
#include <X11/Xlib.h>
#include <vulkan/vulkan.hpp>

#include "vulkan-core.h"
#include "simple-scene.h"


int main(int argc, char *argv[]) {

#ifndef NDEBUG
  std::cout << "Debug mode on" << std::endl;
    const struct vk::core::VkAppConfig config = {
      "Space", "SpaceEngine", {
        "VK_LAYER_LUNARG_standard_validation" },
      { VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME }};
#else
    const struct vk::core::VkAppConfig config = {
      "Space", "SpaceEngine", {},
      {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME}};
#endif
  const unsigned kWidth = 1024;
  const unsigned kHeight = 768;

  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Couldn't open display.");
    return 1;
  }
  int screen = XDefaultScreen(display);
  Window root_window = XRootWindow(display, screen);
  Window window = XCreateSimpleWindow(
    display, root_window, 0, 0, kWidth, kHeight, 0, 0, 0);

  vk::core::VkAppContext vk_ctx;

  if (auto ret = InitVulkan(config, display, window)) {
    vk_ctx = std::move(ret.value());
  } else {
    fprintf(stderr, "Couldn't initialize vulkan.");
    return 1;
  }

  {
    StaticWireframeScene3D scene(&vk_ctx);
  }
  XCloseDisplay(display);
  return 0;
}
