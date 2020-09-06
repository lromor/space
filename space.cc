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
#define XK_LATIN1

#include <getopt.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>

#include <vulkan/vulkan.hpp>

#include "curve.h"
#include "gamepad.h"
#include "reference-grid.h"
#include "scene.h"
#include "vulkan-core.h"

bool GetMasterPointerAndKeyboard(Display *display, int *pointer_device, int *keyboard_device) {
  XIDeviceInfo *info;
  int ndevices;
  info = XIQueryDevice(display, XIAllDevices, &ndevices);
  bool pointer_is_found = false;
  bool keyboard_is_found = false;

  // We select the first master pointer and keyboard.
  for(int i = 0; i < ndevices; ++i) {
    const XIDeviceInfo &dinfo = info[i];
    switch(dinfo.use) {
    case XIMasterPointer: {
      if (!pointer_is_found) {
        *pointer_device = dinfo.deviceid;
#ifndef NDEBUG
        fprintf(stdout, "Found pointer device: %s\n", dinfo.name);
#endif
        pointer_is_found = true;
      }
      break;
    }
    case XIMasterKeyboard: {
      if (!keyboard_is_found) {
        *keyboard_device = dinfo.deviceid;
#ifndef NDEBUG
        fprintf(stdout, "Found keyboard device: %s\n", dinfo.name);
#endif
        keyboard_is_found = true;
      }
      break;
    }
    }
  }
  XIFreeDeviceInfo(info);
  return pointer_is_found && keyboard_is_found;
}

Cursor InvisibleCursor(Display *display, Window window) {
  Cursor invisible_cursor;
  Pixmap no_pixmap;
  XColor black;
  static char nothing[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

  no_pixmap = XCreateBitmapFromData(display, window, nothing, 8, 8);
  invisible_cursor = XCreatePixmapCursor(
    display, no_pixmap, no_pixmap, &black, &black, 0, 0);
  return invisible_cursor;
}

std::pair<double, double> GetRawDataValues(XIRawEvent *event, double *dx, double *dy) {
  double *val = event->valuators.values;
  std::vector<double> values;
  for (int i = 0; i < event->valuators.mask_len * 8; i++) {
    if (XIMaskIsSet(event->valuators.mask, i))
      values.push_back(*val++);
  }
  return { values[0], values[1] };
}

void GetCurrentPointerPosition(Display *display, Window window, int device, double *win_x, double *win_y) {
  Window returned_root, returned_child;
  double root_x, root_y;
  XIButtonState buttons;
  XIModifierState mods;
  XIGroupState group;
  XIQueryPointer(
    display, device, window,
    &returned_root, &returned_child,
    &root_x, &root_y, win_x, win_y, &buttons, &mods, &group);
}

std::optional<vk::Extent2D> get_xlib_window_extent(Display *display, Window window) {
  XWindowAttributes attrs;
  if (XGetWindowAttributes(display, window, &attrs)) {
    return vk::Extent2D(attrs.width, attrs.height);
  }
  return {};
}

static void gamepad2camera(
  CameraControls *camera_controls, const struct EventData &data) {
  if (!data.is_button) {
    switch (data.source) {
    case LEFT_STICK_X:
      camera_controls->dx = -data.value;
      return;
    case LEFT_STICK_Y:
      camera_controls->dy = -data.value;
      return;
    case RIGHT_STICK_X:
      camera_controls->dphi = -data.value;
      return;
    case RIGHT_STICK_Y:
      camera_controls->dtheta = data.value;
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

  int x11_fd = XConnectionNumber(display);
  int screen = XDefaultScreen(display);
  Window root_window = XRootWindow(display, screen);
  Window window = XCreateSimpleWindow(
    display, root_window, 0, 0, kWidth, kHeight, 0, 0, 0);
  XEvent event;
  int xi_opcode, xi_event, xi_error;
  int pointer_device, keyboard_device;
  XIEventMask mask;
  XGenericEventCookie *cookie = (XGenericEventCookie*) &event.xcookie;
  XIDeviceEvent *device_data;
  XIRawEvent *raw_data;
  double last_win_x, last_win_y;

  if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &xi_event, &xi_error)) {
    fprintf(stderr, "X Input extension not available.\n");
    return 1;
  }

  // Find XI2 master pointer and keyboard.
  GetMasterPointerAndKeyboard(display, &pointer_device, &keyboard_device);

  // Enable XI2 events.
  mask.deviceid = XIAllMasterDevices;
  mask.mask_len = XIMaskLen(XI_LASTEVENT);
  mask.mask = new unsigned char[mask.mask_len]();
  XISetMask(mask.mask, XI_ButtonPress);
  XISetMask(mask.mask, XI_ButtonRelease);
  XISetMask(mask.mask, XI_KeyPress);
  XISetMask(mask.mask, XI_KeyRelease);

  XISelectEvents(display, window, &mask, 1);

  XSelectInput(display, window, ExposureMask);
  XMapWindow(display, window);
  XSync(display, False);

  delete mask.mask;

  XMaskEvent(display, ExposureMask, &event);
  XSelectInput(display, window, 0);

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
    Scene scene(&vk_ctx, [display, window] () {
      XWindowAttributes attrs;
      XGetWindowAttributes(display, window, &attrs);
      return vk::Extent2D(attrs.width, attrs.height);
    });

    ReferenceGrid reference_grid;
    Curve curve;

    scene.Init();
    scene.AddEntity(&reference_grid);
    scene.AddEntity(&curve);

    const int max_fd = std::max(x11_fd, gamepad_fd) + 1;
    struct timeval timeout;
    fd_set read_fds;
    bool exit = false;

    CameraControls camera_input = {};

    if (gamepad)
      gamepad->SetEventCallback(
        [&camera_input](const struct EventData &data) {
          gamepad2camera(&camera_input, data);
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
          XNextEvent(display, &event);
          if (XGetEventData(display, cookie) &&
              cookie->type == GenericEvent &&
              cookie->extension == xi_opcode) {
            switch (cookie->evtype) {
            case XI_ButtonPress: {
              device_data = (XIDeviceEvent *)cookie->data;
              if (device_data->detail == Button1) {
                GetCurrentPointerPosition(
                  display, window, pointer_device, &last_win_x, &last_win_y);
                mask.deviceid = pointer_device;
                mask.mask_len = XIMaskLen(XI_LASTEVENT);
                mask.mask = new unsigned char[mask.mask_len]();
                XISetMask(mask.mask, XI_RawMotion);
                XIGrabDevice(display, pointer_device, window, CurrentTime,
                             InvisibleCursor(display, window),
                             XIGrabModeAsync, XIGrabModeAsync, True, &mask);
                delete mask.mask;
                mask.deviceid = keyboard_device;
                mask.mask_len = XIMaskLen(XI_LASTEVENT);
                mask.mask = new unsigned char[mask.mask_len]();
                XISetMask(mask.mask, XI_RawKeyPress);
                XISetMask(mask.mask, XI_RawKeyRelease);
                XIGrabDevice(display, keyboard_device, window, CurrentTime,
                             None, XIGrabModeAsync, XIGrabModeAsync, True, &mask);
                delete mask.mask;
              }
              break;
            }
            case XI_ButtonRelease: {
              device_data = (XIDeviceEvent *)cookie->data;
              if (device_data->detail == Button1) {
                XIWarpPointer(display, pointer_device, window, window, 0, 0, 0, 0, last_win_x, last_win_y);
                XIUngrabDevice(display, pointer_device, CurrentTime);
                XIUngrabDevice(display, keyboard_device, CurrentTime);
              }
              break;
            }
            case XI_KeyPress: {
              device_data = (XIDeviceEvent *)cookie->data;
              if (XkbKeycodeToKeysym(display, device_data->detail, 0, 0) == XK_Escape)
                exit = true;
              break;
            }
            case XI_RawMotion: {
              raw_data = (XIRawEvent *)cookie->data;
              auto values = GetRawDataValues(raw_data, NULL, NULL);
              CameraControls input{0.0f, 0.0f, 0.0f, -(float) values.first, (float) values.second};
              scene.Input(input);
              XIWarpPointer(display, pointer_device, window, window, 0, 0, 0, 0, last_win_x, last_win_y);
              break;
            }
            }
            XFreeEventData(display, cookie);
          }
        }
      }

      if (exit) break;

      auto delta = std::chrono::steady_clock::now() - start;
      if (std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() > 16) {
        scene.Input(camera_input);
        scene.SubmitRendering();
        scene.Present();
      }
    }
  }

  XCloseDisplay(display);
  return 0;
}
