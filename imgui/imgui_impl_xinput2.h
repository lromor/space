#ifndef _IMGUI_IMPL_XINPUT2_H
#define _IMGUI_IMPL_XINPUT2_H

#include <X11/Xlib.h>
#include "input/xinput2.h"
#include "imgui/imgui.h"

IMGUI_IMPL_API bool ImGui_ImplXlib_InitForVulkan(Display* display, Window window);
// To be called whenever a new frame is ready
IMGUI_IMPL_API void ImGui_ImplXlib_NewFrame();

IMGUI_IMPL_API void ImGui_ImplXlib_MouseButtonCallback(Display *window, int button, int action, int mods);
IMGUI_IMPL_API void ImGui_ImplXlib_ScrollCallback(Display *window, double xoffset, double yoffset);
IMGUI_IMPL_API void ImGui_ImplXlib_KeyCallback(Display *window, int key, int scancode, int action, int mods);
IMGUI_IMPL_API void ImGui_ImplXlib_CharCallback(Display *window, unsigned int c);

#endif // _IMGUI_IMPL_XINPUT2_H
