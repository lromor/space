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
//
// This file contains the classes and function definitions for reading events
// from a gamepad using evedev.

#include <string.h>
#include <string>
#include <unistd.h>
#include <iostream>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libevdev-1.0/libevdev/libevdev.h>

#include "gamepad.h"

static EventType MapAxes(uint16_t code) {
  switch (code) {
    case ABS_X: return EventType::kAxisLeftX;
    case ABS_Y: return EventType::kAxisLeftY;
    case ABS_RX: return EventType::kAxisRightX;
    case ABS_RY: return EventType::kAxisRightY;
    default:
      return EventType::kUnknown;
  }
}

static EventType MapButtons(uint16_t code) {
  switch (code) {
    case BTN_SOUTH: return EventType::kButtonSouth;
    case BTN_EAST: return EventType::kButtonEast;
    case BTN_NORTH: return EventType::kButtonNorth;
    case BTN_WEST: return EventType::kButtonWest;
    default:
      return EventType::kUnknown;
  }
}

class Gamepad::Impl {
 public:
  Impl() : fd_(-1), dev_(NULL) {}
  bool Init(const std::string &path);
  ~Impl() {
    libevdev_free(dev_);
    dev_ = NULL;
    close(fd_);
    fd_ = -1;
  }
  int GetFd() { return fd_; }
  void ReadEvents();
  void SetEventCallback(const EventCallback &clbk);
  const bool MapSupportedEvent(
      struct input_event *event, struct EventData *data);
 private:

  void print_stats();

  int fd_;
  struct libevdev *dev_;
  uint8_t axes_number_;
  uint8_t buttons_number_;
  EventCallback clbk_;
};

std::unique_ptr<Gamepad> Gamepad::Create(const std::string &path) {
  std::unique_ptr<Impl> impl = std::make_unique<Impl>();
  if (!impl->Init(path))
    return NULL;
  return std::unique_ptr<Gamepad>(new Gamepad(impl));
}

bool Gamepad::Impl::Init(const std::string &path) {
  int rc = -1;
  fd_ = open(path.c_str(), O_RDONLY | O_NONBLOCK);
  rc = libevdev_new_from_fd(fd_, &dev_);
  if (rc < 0) {
    fprintf(stderr, "Failed to init libevdev (%s).\n", strerror(-rc));
    return false;
  }
  if (!libevdev_has_event_code(dev_, EV_KEY, BTN_GAMEPAD)) {
    printf("This device does not look like a gamepad.\n");
    return false;
  }

#ifndef NDEBUG
  fprintf(stdout, "Gamepad device name: \"%s\"\n", libevdev_get_name(dev_));
#endif

  return true;
}

void Gamepad::Impl::SetEventCallback(const EventCallback &clbk) { clbk_ = clbk; }

const bool Gamepad::Impl::MapSupportedEvent(
    struct input_event *event, struct EventData *data) {
  const uint16_t code = event->code;
  const uint16_t type = event->type;
  const uint16_t value = event->value;
  if (type == EV_KEY) {
    const EventType button_source = MapButtons(code);
    if (button_source == EventType::kUnknown)
      return false;
    data->type = button_source;
    data->value = value;
    return true;
  }
  if (type == EV_ABS) {
    const EventType axis_source = MapAxes(code);
    if (axis_source == EventType::kUnknown)
      return false;
    const struct input_absinfo *abs =
        libevdev_get_abs_info(dev_, code);
    data->type = axis_source;
    const int delta = (abs->maximum - abs->minimum);
    data->value = value * 2.0 / delta - 1.0;
    return true;
  }
  return false;
}

void Gamepad::Impl::ReadEvents() {
  struct input_event event;
  int rc, flag = LIBEVDEV_READ_FLAG_NORMAL;
  struct EventData data;
  while ((rc = libevdev_next_event(dev_, flag, &event))
         != -EAGAIN) {
    if (event.type == EV_SYN
        && event.code == SYN_DROPPED) {
      flag = LIBEVDEV_READ_FLAG_SYNC;
      continue;
    }
    // The event we received is not one we are yet interested in
    // Let's skip it.
    if (!MapSupportedEvent(&event, &data))
      continue;

    // Everything else is mapped, and make a call
    // to the callbacks. We don't want to lose any
    // event otherwise we might experience some
    // "drifting".
    clbk_(data);
  }
}

const int Gamepad::GetFd() { return impl_->GetFd(); }

void Gamepad::SetEventCallback(const EventCallback &clbk) {
  return impl_->SetEventCallback(clbk);
}

void Gamepad::ReadEvents() { return impl_->ReadEvents(); }

Gamepad::~Gamepad() {}
Gamepad::Gamepad(std::unique_ptr<Impl> &impl) : impl_(std::move(impl)) {}
