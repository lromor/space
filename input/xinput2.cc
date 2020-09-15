#include <X11/X.h>
#include <X11/extensions/XInput2.h>
#include <X11/XKBlib.h>

#include <cstdio>
#include "xinput2.h"

static bool GetMasterPointerAndKeyboard(Display *display, int *pointer_device, int *keyboard_device) {
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

static std::pair<double, double> GetRawDataValues(XIRawEvent *event, double *dx, double *dy) {
  double *val = event->valuators.values;
  std::vector<double> values;
  for (int i = 0; i < event->valuators.mask_len * 8; i++) {
    if (XIMaskIsSet(event->valuators.mask, i))
      values.push_back(*val++);
  }
  return { values[0], values[1] };
}

void GetCurrentPointerPosition(
  Display *display, Window window, int device,
  double *win_x, double *win_y) {
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

bool XInput2::Init() {
  XIEventMask mask;
  if (!XQueryExtension(display_, "XInputExtension", &xi_opcode_, &xi_event_, &xi_error_)) {
    fprintf(stderr, "X Input extension not available.\n");
    return false;
  }

  // Find XI2 master pointer and keyboard.
  GetMasterPointerAndKeyboard(display_, &pointer_device_, &keyboard_device_);

  // Enable XI2 events.
  mask.deviceid = XIAllMasterDevices;
  mask.mask_len = XIMaskLen(XI_LASTEVENT);
  mask.mask = new unsigned char[mask.mask_len]();
  XISetMask(mask.mask, XI_ButtonPress);
  XISetMask(mask.mask, XI_ButtonRelease);
  XISetMask(mask.mask, XI_KeyPress);
  XISetMask(mask.mask, XI_KeyRelease);

  XISelectEvents(display_, window_, &mask, 1);
  XSync(display_, False);

  delete mask.mask;
  return true;
}

void XInput2::GrabPointerDevice() {
  XIEventMask mask;
  mask.deviceid = pointer_device_;
  mask.mask_len = XIMaskLen(XI_LASTEVENT);
  mask.mask = new unsigned char[mask.mask_len]();
  XISetMask(mask.mask, XI_RawMotion);
  XISetMask(mask.mask, XI_ButtonPress);
  XISetMask(mask.mask, XI_ButtonRelease);

  XIGrabDevice(display_, pointer_device_, window_, CurrentTime,
               None,
               XIGrabModeAsync, XIGrabModeAsync, True, &mask);
  delete mask.mask;
}

void XInput2::UngrabPointerDevice() {
  XIUngrabDevice(display_, pointer_device_, CurrentTime);
}

void XInput2::ReadEvents(XEvent event) {
  XGenericEventCookie *cookie = (XGenericEventCookie*) &event.xcookie;
  XIDeviceEvent *device_data;
  XIRawEvent *raw_data;
  if (XGetEventData(display_, cookie) &&
      cookie->type == GenericEvent &&
      cookie->extension == xi_opcode_) {
    switch (cookie->evtype) {
    case XI_ButtonPress: {
      device_data = (XIDeviceEvent *)cookie->data;
      const unsigned int button = device_data->detail;
      if (button == Button4 || button == Button5 ) {
        const float value = (button == Button4) ? 1.0f : -1.0f;
        for (const auto &c : on_wheel_event_)
          c(this, value);
        break;
      }
      for (const auto &c : on_button_event_)
        c(this, KeyButtonEventType::kButtonPressed, device_data->detail);

      break;
    }
    case XI_ButtonRelease: {
      device_data = (XIDeviceEvent *)cookie->data;
      for (const auto &c : on_button_event_)
        c(this, KeyButtonEventType::kButtonReleased, device_data->detail);
      break;
    }
    case XI_KeyPress: {
      device_data = (XIDeviceEvent *)cookie->data;
      for (const auto &c : on_key_event_)
        c(this, KeyButtonEventType::kButtonPressed,
          XkbKeycodeToKeysym(display_, device_data->detail, 0, 0));
      break;
    }
    case XI_KeyRelease: {
      device_data = (XIDeviceEvent *)cookie->data;
      for (const auto &c : on_key_event_)
        c(this, KeyButtonEventType::kButtonReleased,
          XkbKeycodeToKeysym(display_, device_data->detail, 0, 0));
      break;
    }
    case XI_RawMotion: {
      raw_data = (XIRawEvent *)cookie->data;
      auto values = GetRawDataValues(raw_data, NULL, NULL);
      for (const auto &c : on_pointer_motion_event_)
        c(this, values.first, values.second);
      break;
    }
    }
    XFreeEventData(display_, cookie);
  }
}

void XInput2::GetAbsolutePointerPosition(double *x, double *y)
  { GetCurrentPointerPosition(display_, window_, pointer_device_, x, y); }
