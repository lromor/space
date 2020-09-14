#ifndef __INTERFACE_MANAGER_H_
#define __INTERFACE_MANAGER_H_

#include <X11/Xlib.h>
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

private:
  Display *display_;
  Window window_;
  Scene *scene_;
  Camera *camera_;
  XInput2 *xinput2_;
  Gamepad *gamepad_;

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
#endif // __INTERFACE_MANAGER_H_
