
#include <iostream>
#include <optional>
#include <X11/Xlib.h>
#include <vulkan/vulkan.hpp>

#include "vulkan-core.h"
#include "simple-scene.h"


/*  7----6
   /|   /|
  4----5 |
  | 3--|-2
  |/   |/
  0----1
*/


static const std::vector<Vertex> cube_vertices = {
  // X, Y, Z, W
  // Lower vertices
  {-1.0f, -1.0f,  1.0f, 1.0f}, // 0
  { 1.0f, -1.0f,  1.0f, 1.0f}, // 1
  { 1.0f, -1.0f, -1.0f, 1.0f}, // 2
  {-1.0f, -1.0f, -1.0f, 1.0f}, // 3

  // Upper vertices
  {-1.0f,  1.0f,  1.0f, 1.0f}, // 4
  { 1.0f,  1.0f,  1.0f, 1.0f}, // 5
  { 1.0f,  1.0f, -1.0f, 1.0f}, // 6
  {-1.0f,  1.0f, -1.0f, 1.0f}, // 7
};


// The indices from the pipeline configuration
// can be configured to be both clockwise or counter clockwise.
// In this case we are using clockwise indices to define the face.
// See https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkFrontFace.html
static const std::vector<uint16_t> cube_indexes = {
  0, 4, 1,
  1, 4, 5,
  1, 5, 2,
  2, 5, 6,
  2, 3, 6,
  6, 3, 7,
  3, 7, 4,
  0, 3, 4,
  4, 7, 5,
  5, 7, 6,
  0, 3, 1,
  1, 3, 2
};


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

    Mesh mesh{ cube_vertices, cube_indexes };
    scene.AddMesh(mesh);

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    XEvent e;
    for (;;) {
      XNextEvent(display, &e);
      if (e.type == KeyPress)
         break;
   }
  }
 
  XCloseDisplay(display);
  return 0;
}
