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

#define XK_MISCELLANY

#include <getopt.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/keysymdef.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>

#include <vulkan/vulkan.hpp>

#include "curve.h"
#include "gamepad.h"
#include "reference-grid.h"
#include "scene.h"
#include "vulkan-core.h"


static void gamepad2camera(
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

  int x11_fd = ConnectionNumber(display);
  int screen = XDefaultScreen(display);
  Window root_window = XRootWindow(display, screen);
  Window window = XCreateSimpleWindow(
    display, root_window, 0, 0, kWidth, kHeight, 0, 0, 0);

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
    Scene scene(&vk_ctx);
    ReferenceGrid reference_grid;
    Curve curve;

    scene.AddEntity(&reference_grid);
    scene.AddEntity(&curve);
    XSelectInput(display, window,
                 ExposureMask
                 | KeyPressMask
                 | PointerMotionMask);
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
          gamepad2camera(&controls, data);
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
          if (e.type == KeyPress || e.type == KeyRelease) {
            const bool isKeyReleased = (e.type == KeyRelease);
            const KeySym keysym = XLookupKeysym((XKeyEvent*) &e, 0);
            fprintf(stderr, "key-%s %lx\n", e.type == KeyPress ? "dn" : "up",
                    keysym);
            const float move_value = isKeyReleased ? 0 : 0.1;
            switch (keysym) {
            case XK_Right:
              gamepad2camera(&controls, { -move_value, RIGHT_STICK_X, false});
              break;

            case XK_Left:
              gamepad2camera(&controls, { +move_value, RIGHT_STICK_X, false});
              break;

            case XK_Up:
              gamepad2camera(&controls, { +move_value, RIGHT_STICK_Y, false});
              break;

            case XK_Down:
              gamepad2camera(&controls, { -move_value, RIGHT_STICK_Y, false});
              break;

            case XK_Escape:
              exit = true;
              break;
            }
          }
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
