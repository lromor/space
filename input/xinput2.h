#ifndef _XINPUT2_H
#define _XINPUT2_H

#include <X11/Xlib.h>
#include <functional>
#include <vector>

// XInput2 event handler. It's associated to a specific window.
class XInput2 {
public:
  enum class KeyButtonEventType { kButtonPressed, kButtonReleased };

  typedef std::function<
    void(XInput2*, const KeyButtonEventType, const unsigned int)> KeyButtonCallback;
  typedef std::function<
    void(XInput2*, float dz)> WheelCallback;
  typedef std::function<
    void(XInput2*, double dx, double dy)> PointerMotionCallback;

  XInput2(Display *display, Window window)
    : display_(display), window_(window),
      pointer_device_(-1), keyboard_device_(-1) {}

  // Initialize devices and masks
  bool Init();

  // Register event listeners.
  void OnKeyEvent(const KeyButtonCallback &callback)
    { on_key_event_.push_back(callback); }
  void OnButtonEvent(const KeyButtonCallback &callback)
    { on_button_event_.push_back(callback); }
  void OnWheelEvent(const WheelCallback &callback)
    { on_wheel_event_.push_back(callback); }
  void OnPointerMotionEvent(const PointerMotionCallback &callback)
    { on_pointer_motion_event_.push_back(callback); }

  // Public interface to query input state.
  void GrabPointerDevice();
  void UngrabPointerDevice();

  void GetAbsolutePointerPosition(double *x, double *y);
  bool GetButtonState(unsigned int button);


  // Handle stuff like CTRL, ALT, etc...
  void GetModifierKeys() {}

  // Same FD as the original display connection.
  const int GetFd() { return XConnectionNumber(display_); }

  // Read an xlib event.
  void ReadEvents(XEvent event);

private:
  Display *display_;
  Window window_;
  int pointer_device_, keyboard_device_;
  int xi_opcode_, xi_event_, xi_error_;

  std::vector<KeyButtonCallback> on_key_event_;
  std::vector<KeyButtonCallback> on_button_event_;
  std::vector<WheelCallback> on_wheel_event_;
  std::vector<PointerMotionCallback> on_pointer_motion_event_;
};

#endif // _XINPUT2_H
