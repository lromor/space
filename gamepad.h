
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

// If is_button is true, the
// button if pressed if value > 0
// The struct represent the normalized values of
// the gamepad.
struct EventData {
  float value;
  int source;
  bool is_button;
};



// Follows the Linux Gamepad specification
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
  void ReadEvents();

private:
  class Impl;
  Gamepad(std::unique_ptr<Impl> &impl);
  std::unique_ptr<Impl> impl_;
};


#endif // _GAMEPAD_H
