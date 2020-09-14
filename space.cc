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
#include "input/xinput2.h"
#include <X11/X.h>
#include <algorithm>
#define XK_MISCELLANY
#define XK_LATIN1

#include <getopt.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>

#include <vulkan/vulkan.hpp>

#include "camera.h"
#include "curve.h"
#include "input/gamepad.h"
#include "reference-grid.h"
#include "scene.h"
#include "vulkan-core.h"
#include "interface-manager.h"

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
#endif

  const struct space::core::VkAppConfig config = {
    "Space", "SpaceEngine", {},
    { VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_XLIB_SURFACE_EXTENSION_NAME }};

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

  XInitThreads();
  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Couldn't open display.");
    return 1;
  }

  int x11_fd = XConnectionNumber(display);
  int screen = XDefaultScreen(display);
  Window root_window = XRootWindow(display, screen);
  Window window = XCreateSimpleWindow(
    display, root_window, 0, 0, kWidth, kHeight, 0, 0, 0);
  XEvent event;

  XSelectInput(display, window, ExposureMask);
  XMapWindow(display, window);
  XSync(display, False);

  XMaskEvent(display, ExposureMask, &event);
  XSelectInput(display, window, 0);

  auto get_window_extent = [display, window] () {
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);
    return vk::Extent2D(attrs.width, attrs.height);
  };

  // Initialize vulkan
  space::core::VkAppContext vk_ctx;

  if (auto ret = InitVulkan(config, display, window)) {
    vk_ctx = std::move(ret.value());
  } else {
    fprintf(stderr, "Couldn't initialize vulkan.");
    return 1;
  }

  // We create the scene inside the scope
  // as we want to avoid its destruction after
  // the display is closed with XCloseDisplay().
  {
    Camera camera;
    Scene scene(&vk_ctx, &camera, get_window_extent);

    ReferenceGrid reference_grid;
    Curve curve;

    scene.Init();
    scene.AddEntity(&reference_grid);
    scene.AddEntity(&curve);

    const int max_fd = std::max(x11_fd, gamepad_fd) + 1;
    struct timeval timeout;
    fd_set read_fds;

    // X11 Keyboard and mouse event utility class
    XInput2 xinput2(display, window);
    xinput2.Init();

    // We should substitute this with a proper
    // Input manager class which takes care of
    // dispatching events to the scene in case
    // complex internal states should be ever required.
    InterfaceManager interface_manager(display, window, &scene, &camera, &xinput2, gamepad.get());

    // Get file descriptor for when the rendering is done to add
    // to be added to select.
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
          XNextEvent(display, &event);
          xinput2.ReadEvents(event);
        }
      }
      if (interface_manager.Exit()) break;

      auto delta = std::chrono::steady_clock::now() - start;
      if (std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() > 16) {
        scene.SubmitRendering();
        scene.Present();
      }
    }
  }

  XCloseDisplay(display);
  return 0;
}
