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
#include "interface-manager.h"
#include <cmath>

#define XK_MISCELLANY
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

#include "input/gamepad.h"

InterfaceManager::InterfaceManager(Display *display, Window window,
                                   Scene *scene, Camera *camera, XInput2 *xinput2,
                                   Gamepad *gamepad)
    : display_(display), window_(window), scene_(scene),
      camera_(camera), xinput2_(xinput2), gamepad_(gamepad),
      gamepad_state_({}), mouse_state_(0), exit_(false) {

  // Register events for gamepad
  if (gamepad)
    gamepad->SetEventCallback(
        [&](const struct EventData &data) {
          const float value = (std::fabs(data.value) < 5e-2) ? 0 : data.value;
          switch(data.type) {
            case EventType::kAxisLeftX:
              gamepad_state_.axis_left_x_ = value;
              break;
            case EventType::kAxisLeftY:
              gamepad_state_.axis_left_y_ = value;
              break;
            case EventType::kAxisRightX:
              gamepad_state_.axis_right_x_ = value;
              break;
            case EventType::kAxisRightY:
              gamepad_state_.axis_right_y_ = value;
              break;
            default: break;
          }
        });

  // Register mouse event callbacks
  xinput2->OnButtonEvent([&](XInput2 *xinput2,
                             const XInput2::KeyButtonEventType e,
                             const unsigned int b) {
    if (e == XInput2::KeyButtonEventType::kButtonReleased)
      DisableDragMode();
    else {
      switch (b) {
        case Button1:
          EnableDragMode1();
          break;
        case Button3:
          EnableDragMode2();
          break;
      }
    }
  });

  xinput2->OnPointerMotionEvent([&](
      XInput2 *xinput2,
      double dx, double dy) {
    DispatchPointerMotionEvent(dx, dy);
  });

  xinput2->OnWheelEvent([&](XInput2 *xinput2, float dz) {
    DispatchWheelEvent(dz);
  });

  xinput2->OnKeyEvent([&](XInput2 *xinput2,
                          const XInput2::KeyButtonEventType e,
                          const unsigned int b) {
    DispatchKeyPressEvent(b);
  });
}

void InterfaceManager::UpdateScene(double dt) {
  camera_->FirstPersonControlMove(gamepad_state_.axis_left_x_ * dt, gamepad_state_.axis_left_y_ * dt);
  camera_->FirstPersonControlRotate(gamepad_state_.axis_right_x_ * dt, gamepad_state_.axis_right_y_ * dt);
}

void InterfaceManager::EnableDragMode1() {
  if (mouse_state_ == IDLE) {
    xinput2_->GrabPointerDevice();
    mouse_state_ = DRAG_MODE1;
  }
}

void InterfaceManager::EnableDragMode2() {
  if (mouse_state_ == IDLE) {
    xinput2_->GrabPointerDevice();
    mouse_state_ = DRAG_MODE2;
  }
}

void InterfaceManager::DisableDragMode() {
  if (mouse_state_ != IDLE) {
    xinput2_->UngrabPointerDevice();
    mouse_state_ = IDLE;
  }
}

void InterfaceManager::DispatchPointerMotionEvent(double dx, double dy) {
  XWindowAttributes attrs;
  const float kPanSpeed = 5.0f;
  const float kRotateSpeed = 2.0f;

  XGetWindowAttributes(display_, window_, &attrs);
  const unsigned int width = attrs.width;
  const unsigned int height = attrs.height;
  const int minor_axis = std::min(width, height) / 2;
  const float dside = dx / minor_axis;
  const float dup = -dy / minor_axis;

  switch (mouse_state_) {
    case DRAG_MODE1:
      camera_->TrackballControlRotate(dside * kRotateSpeed, dup * kRotateSpeed);
      break;
    case DRAG_MODE2:
      camera_->TrackballControlPan(dside * kPanSpeed, dup * kPanSpeed);
      break;
  }
}

void InterfaceManager::DispatchWheelEvent(float dz) {
  camera_->TrackballControlZoom(dz);
}

void InterfaceManager::DispatchKeyPressEvent(unsigned int keysim) {
  switch (keysim) {
    case XK_Escape:
      exit_ = true;
      break;
  }
}
