
#include <iostream>
#include <optional>
#include <memory>
#include <chrono>
#include <getopt.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <vulkan/vulkan.hpp>

#include "vulkan-core.h"
#include "simple-scene.h"
#include "gamepad.h"


static void update_camera_controls(
  CameraControls *camera_controls, const struct EventData &data) {
  if (!data.is_button) {
    switch (data.source) {
    case LEFT_STICK_X:
      camera_controls->dx = data.value;
      return;
    case LEFT_STICK_Y:
      camera_controls->dy = -1 * data.value;
      return;
    case RIGHT_STICK_X:
      camera_controls->dphi = data.value;
      return;
    case RIGHT_STICK_Y:
      camera_controls->dtheta = -1 * data.value;
      return;
    default:
      return;
    }
    return;
  }
}

/*  6----7
   /|   /|
  3----2 |
  | 5--|-4
  |/   |/
  0----1
*/

static const std::vector<Vertex> cube_vertices = {
  // X, Y, Z, W
  // Front vertices
  { -1.0f, -1.0f, -1.0f, 1.0f}, // 0
  {  1.0f, -1.0f, -1.0f, 1.0f}, // 1
  {  1.0f,  1.0f, -1.0f, 1.0f}, // 2
  { -1.0f,  1.0f, -1.0f, 1.0f}, // 3

  // Back vertices
  {  1.0f, -1.0f,  1.0f, 1.0f}, // 4
  { -1.0f, -1.0f,  1.0f, 1.0f}, // 5
  { -1.0f,  1.0f,  1.0f, 1.0f}, // 6
  {  1.0f,  1.0f,  1.0f, 1.0f}, // 7
};


// The indices from the pipeline configuration
// can be configured to be both clockwise or counter clockwise.
// In this case we are using clockwise indices to define the face.
// See https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkFrontFace.html
static const std::vector<uint16_t> cube_indexes = {
  0, 1, 3,
  1, 2, 3,
  1, 4, 2,
  4, 2, 7,
  4, 5, 7,
  7, 5, 6,
  6, 5, 3,
  5, 0, 3,
  3, 2, 6,
  2, 7, 6,
  0, 1, 5,
  1, 4, 5
};

static int usage(const char *prog, const char *msg) {
  if (msg) {
    fprintf(stderr, "\033[1m\033[31m%s\033[0m\n\n", msg);
  }
  fprintf(stderr, "Usage: %s [options]\n"
          "Options:\n", prog);
  fprintf(stderr,
          "\t    --gamepad <path>     : Use a gamepad as external controller.\n"
          "\t-h, --help               : Display this help text and exit.\n");
  return 1;
}


int main(int argc, char *argv[]) {
  std::string gamepad_path;

  enum LongOptionsOnly {
    OPT_GAMEPAD = 1000
  };

  static struct option long_options[] = {
    { "help",    no_argument,       NULL, 'h' },
    { "gamepad", required_argument, NULL, OPT_GAMEPAD },
    { 0,         0,                 0,    0  },
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
    switch (opt) {
    case 'h':
      return usage(argv[0], NULL);
    case OPT_GAMEPAD:
      gamepad_path = std::string(optarg);
      break;
    default:
      return usage(argv[0], "Unkown or invalid option.");
    }
  }

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

  std::unique_ptr<Gamepad> gamepad;
  int gamepad_fd = -1;
  if (!gamepad_path.empty()) {
    gamepad = Gamepad::Create(gamepad_path);
    if (gamepad) {
      gamepad_fd = gamepad->GetFd();
      assert(gamepad_fd >= 0);
    }
  }

  const unsigned kWidth = 1024;
  const unsigned kHeight = 768;

  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Couldn't open display.");
    return 1;
  }

  int x11_fd = ConnectionNumber(display);
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

  // We the scene shouldn't be destroyed after we close the
  // display.
  {
    StaticWireframeScene3D scene(&vk_ctx);

    Mesh mesh{ cube_vertices, cube_indexes };
    scene.AddMesh(mesh);

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    XFlush(display);

    const int max_fd = std::max(x11_fd, gamepad_fd) + 1;
    struct timeval timeout;
    fd_set read_fds;
    XEvent e;
    bool exit = false;

    CameraControls controls = {};
    if (gamepad)
      gamepad->SetEventCallback(
        [&controls](const struct EventData &data) {
          update_camera_controls(&controls, data);
        });

    auto start = std::chrono::steady_clock::now();

    for (;;) {

      FD_ZERO(&read_fds);
      FD_SET(x11_fd, &read_fds);
      if (gamepad_fd > 0)
        FD_SET(gamepad_fd, &read_fds);

      timeout.tv_usec = 1000;
      timeout.tv_sec = 0;
      int fds_ready = select(max_fd, &read_fds, NULL, NULL, &timeout);

      if (fds_ready < 0) {
        perror("select() failed");
        return 1;
      }

      if (gamepad && FD_ISSET(gamepad_fd, &read_fds)) {
        gamepad->ReadEvents();
      }

      if (FD_ISSET(x11_fd, &read_fds)) {
        while(XPending(display)) {
          XNextEvent(display, &e);
          if (e.type == KeyPress)
            exit = true;
        }
      }

      if (exit) break;

      auto delta = std::chrono::steady_clock::now() - start;
      if (std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() > 16) {
        scene.Input(controls);
        scene.SubmitRendering();
        scene.Present();
      }


   }
  }
 
  XCloseDisplay(display);
  return 0;
}
