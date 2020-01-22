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

#ifndef _GAMEPAD_H
#define _GAMEPAD_H

#include <memory>
#include <functional>

enum Axis {
  LEFT_STICK_X = 0,
  LEFT_STICK_Y,
  RIGHT_STICK_X,
  RIGHT_STICK_Y,
  NUM_SUPPORTED_AXES
};

enum Button {
  ACTION_BUTTON_SOUTH = 0,
  ACTION_BUTTON_EAST,
  ACTION_BUTTON_NORTH,
  ACTION_BUTTON_WEST,
  NUM_SUPPORTED_BUTTONS
};

// If 'is_button' is true, the
// button is pressed if value > 0.
// Source field is a value of the enum Button.
// If 'is_button' is false, the struct
// represents the normalized values of the axis.
// Source field is a value of the enum Axis.
struct EventData {
  float value; // The normalized value of the axis/button.
  int source; // Either Axis or Button type.
  bool is_button; // True if the event is from a button,
                  // false if from an axis.
};

// Simple event-driven class to read events
// from a compatible gamecontroller evdev linux interface.
class Gamepad {
public:
  ~Gamepad();

  // Create a Gamepad by passing the path of a /dev/input/event<x>
  // file.
  static std::unique_ptr<Gamepad> Create(const std::string &path);

  // Useful if you want to use select() or poll() and then call ReadEvent()
  const int GetFd();

  typedef std::function<void(const struct EventData &data)> EventCallback;
  void SetEventCallback(const EventCallback &clbk);

  // Used to read data from the gamepad. If there's data,
  // the callbacks are triggered.
  // The call is always non blocking.
  void ReadEvents();

private:
  class Impl;
  Gamepad(std::unique_ptr<Impl> &impl);
  std::unique_ptr<Impl> impl_;
};


#endif // _GAMEPAD_H
