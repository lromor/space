
#include <optional>
#include <vulkan/vulkan.hpp>
#include <X11/Xlib.h>

#include "vulkan-core.h"

// Given a physical device search for queues that can both "present" and do "graphics"
// If possible pick a family that support both.
std::optional<std::pair<uint32_t, uint32_t>> FindGraphicsAndPresentQueueFamilyIndex(
  vk::PhysicalDevice physical_device, vk::SurfaceKHR const& surface) {

  std::vector<vk::QueueFamilyProperties> queue_family_properties =
    physical_device.getQueueFamilyProperties();
  assert(queue_family_properties.size() < std::numeric_limits<uint32_t>::max());

  // get the first index into queueFamiliyProperties which supports graphics
  const size_t graphics_queue_family_index =
    std::distance(queue_family_properties.begin(),
                  std::find_if(queue_family_properties.begin(),
                               queue_family_properties.end(),
                               [](vk::QueueFamilyProperties const& qfp) {
                                 return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                               }));
  assert(graphics_queue_family_index < queue_family_properties.size());

  if (physical_device.getSurfaceSupportKHR(graphics_queue_family_index, surface)) {
    // the first graphics_queue_family_index does also support presents
    return std::make_pair(graphics_queue_family_index, graphics_queue_family_index);
  }

  // the graphics_queue_family_index doesn't support present
  // -> look for an other family index that supports both graphics and present
  for (size_t i = 0; i < queue_family_properties.size(); i++) {
    if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
        physical_device.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface)) {
      return std::make_pair(static_cast<uint32_t>(i), static_cast<uint32_t>(i));
    }
  }

  // there's nothing like a single family index that supports both graphics and present
  // -> look for an other family index that supports present
  for (size_t i = 0; i < queue_family_properties.size(); i++) {
    if (physical_device.getSurfaceSupportKHR(static_cast<uint32_t>(i), surface)) {
      return std::make_pair(graphics_queue_family_index, static_cast<uint32_t>(i));
    }
  }

  // Couldn't find both present and graphics queues.
  return {};
}

std::optional<vk::Extent2D> get_xlib_window_extent(Display *display, Window window) {
  XWindowAttributes attrs;
  if (XGetWindowAttributes(display, window, &attrs)) {
    return vk::Extent2D(attrs.width, attrs.height);
  }
  return {};
}

vk::UniqueDevice CreateDevice(vk::PhysicalDevice physical_device, uint32_t graphics_queue,
                              std::vector<const char*> const& extensions) {
  std::vector<char const*> enabled_extensions;
  enabled_extensions.reserve(extensions.size());
  for (auto const& ext : extensions) { enabled_extensions.push_back(ext); }

  // create a UniqueDevice
  float kQueuePriority = 0.0f;
  vk::DeviceQueueCreateInfo device_queue_create_info(
    vk::DeviceQueueCreateFlags(), graphics_queue, 1, &kQueuePriority);

  vk::DeviceCreateInfo device_create_info(
    vk::DeviceCreateFlags(), 1, &device_queue_create_info,
    0, nullptr, enabled_extensions.size(), enabled_extensions.data());

  return physical_device.createDeviceUnique(device_create_info);
}

std::optional<vk::SurfaceFormatKHR> PickSurfaceFormat(
  std::vector<vk::SurfaceFormatKHR> const& formats) {
  assert(!formats.empty());
  vk::SurfaceFormatKHR picked_format = formats[0];
  if (formats.size() == 1) {
    if (formats[0].format == vk::Format::eUndefined) {
      picked_format.format = vk::Format::eB8G8R8A8Unorm;
      picked_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    }
  } else {
    // request several formats, the first found will be used
    vk::Format requested_formats[]  = {
      vk::Format::eB8G8R8A8Unorm,
      vk::Format::eR8G8B8A8Unorm,
      vk::Format::eB8G8R8Unorm,
      vk::Format::eR8G8B8Unorm
    };
    vk::ColorSpaceKHR requestedColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    for (size_t i = 0; i <
           sizeof(requested_formats) / sizeof(requested_formats[0]); i++) {
      vk::Format requestedFormat = requested_formats[i];
      auto it = std::find_if(formats.begin(),
                             formats.end(),
                             [requestedFormat, requestedColorSpace](auto const& f) {
                               return (f.format == requestedFormat)
                                 && (f.colorSpace == requestedColorSpace);
                             });
      if (it != formats.end()) {
        picked_format = *it;
        break;
      }
    }
  }
  assert(picked_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
  return picked_format;
}

std::optional<vk::PresentModeKHR> PickPresentMode(
  std::vector<vk::PresentModeKHR> const& present_modes) {
  vk::PresentModeKHR picked_mode = vk::PresentModeKHR::eFifo;
  for(const auto& present_mode : present_modes) {
    if(present_mode == vk::PresentModeKHR::eMailbox) {
      picked_mode = present_mode;
      break;
    }

    if(present_mode == vk::PresentModeKHR::eImmediate) {
      picked_mode = present_mode;
    }
  }
  return picked_mode;
}

namespace vk {
  namespace core {

    std::optional<VkAppContext> InitVulkan(const VkAppConfig config, Display *display, Window window) {
      vk::ApplicationInfo application_info(
        config.app_name, 1, config.engine_name, 1, VK_API_VERSION_1_1);

      vk::InstanceCreateInfo instance_create_info(
        {}, &application_info, config.instance_layers.size(), config.instance_layers.data(),
        config.instance_extensions.size(), config.instance_extensions.data());

      vk::UniqueInstance instance = vk::createInstanceUnique(instance_create_info);

      // For now we just pick the first device. We should improve the code by
      // searching for the device that has best properties or any other policy
      // possibly configurable policy.
      vk::PhysicalDevice physical_device = instance->enumeratePhysicalDevices().front();

      vk::Extent2D extent;
      if (auto res = get_xlib_window_extent(display, window)) {
        extent = res.value();
      } else {
        fprintf(stderr, "Coudldn't guess the xlib window extent.");
        return {};
      }
      // Init the surface
      struct SurfaceData surface_data = {
        instance->createXlibSurfaceKHRUnique(
          vk::XlibSurfaceCreateInfoKHR(vk::XlibSurfaceCreateFlagsKHR(), display, window)),
        extent};

      // Find devices for present and graphics.
      std::pair<uint32_t, uint32_t> graphics_and_present_queue_family_index;
      if (const auto o = FindGraphicsAndPresentQueueFamilyIndex(physical_device, *surface_data.surface)) {
        graphics_and_present_queue_family_index = o.value();
      } else {
        fprintf(stderr, "Couldn't find suitable Present or Graphics queues.");
        return {};
      }

      // Create logical device. This can enable another set of extensions.
      const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
      };

      vk::UniqueDevice device = CreateDevice(
        physical_device, graphics_and_present_queue_family_index.first,
        device_extensions);

      return VkAppContext{
        std::move(instance), physical_device, std::move(surface_data),
          graphics_and_present_queue_family_index.first,
          graphics_and_present_queue_family_index.second,
          std::move(device)};
    }
  }
}
