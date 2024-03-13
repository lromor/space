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
#ifndef _INTERFACE_MANAGER_H
#define _INTERFACE_MANAGER_H

#include <X11/Xlib.h>
#include <chrono>
#include "scene.h"
#include "input/xinput2.h"
#include "input/gamepad.h"
#include "camera.h"

class InterfaceManager {
 public:
  InterfaceManager(Display *display, Window window,
                   Scene *scene, Camera *camera,
                   XInput2 *xinput2, Gamepad *gamepad);

  bool Exit() { return exit_; }
  void UpdateScene(double dt);

 private:
  Display *display_;
  Window window_;
  Scene *scene_;
  Camera *camera_;
  XInput2 *xinput2_;
  Gamepad *gamepad_;

  struct GamepadState {
    float axis_left_x_;
    float axis_left_y_;
    float axis_right_x_;
    float axis_right_y_;
  } gamepad_state_;

  enum MouseStateValue {
    IDLE,
    DRAG_MODE1,
    DRAG_MODE2
  };

  int mouse_state_;
  bool exit_;

  // Mouse pointer stuff
  void EnableDragMode1();
  void EnableDragMode2();
  void DisableDragMode();
  void DispatchPointerMotionEvent(double dx, double dy);
  void DispatchWheelEvent(float dz);
  void DispatchKeyPressEvent(unsigned int keysim);

};
#endif // _INTERFACE_MANAGER_H
