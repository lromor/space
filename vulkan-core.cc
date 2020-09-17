// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright(c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// Modifications copyright (C) 2020 Leonardo Romor <leonardo.romor@gmail.com>
//
// This file contains all the utilities to instantiate a basic
// vulkan context. A vulkan context is just a container of all the required
// vulkan handles (instance, physical devices, logical devices) required
// to perform rendering/compute/present operations.

#include <cstdio>
#include <iostream>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <X11/Xlib.h>

#include "vulkan-core.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

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

vk::UniqueDevice CreateDevice(
  vk::PhysicalDevice physical_device, uint32_t queue_family_index,
  std::vector<std::string> const& extensions = {},
  vk::PhysicalDeviceFeatures const* physical_device_features = NULL,
  void const* p_next = NULL, float queue_priority = 0.0f) {
  std::vector<char const*> enabled_extensions;
  enabled_extensions.reserve(extensions.size());
  for (auto const& ext : extensions) { enabled_extensions.push_back(ext.data()); }

  // create a UniqueDevice
  vk::DeviceQueueCreateInfo device_queue_create_info(
    vk::DeviceQueueCreateFlags(), queue_family_index, 1, &queue_priority);

  vk::DeviceCreateInfo device_create_info(
    vk::DeviceCreateFlags(), 1, &device_queue_create_info,
    0, nullptr, enabled_extensions.size(), enabled_extensions.data(),
    physical_device_features);

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

// Requires as input a vector of strings of requested
// layers and extensions. Returns the pair of layers and extensions objects
// available in the vulkan implementation.
static std::pair<std::vector<std::string>, std::vector<std::string>> GetInstanceEnabledLayersAndExtensions(
  std::vector<std::string> requested_layers,
  std::vector<std::string> requested_extensions) {
  std::vector<vk::LayerProperties> layers = vk::enumerateInstanceLayerProperties();
  std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties();

  std::vector<std::string> enabled_layers;
  enabled_layers.reserve(requested_layers.size());
  for (auto const& layer : requested_layers) {
    const bool layer_is_present = std::find_if(
      layers.begin(), layers.end(),
      [layer](vk::LayerProperties const& lp) {
        return layer == lp.layerName;
      }) != layers.end();

    if (!layer_is_present) {
      fprintf(stderr, "Could not find Vulkan instance layer: %s\n", layer.c_str());
      continue;
    }

    enabled_layers.push_back(layer.data());
  }

  std::vector<std::string> enabled_extensions;
  enabled_extensions.reserve(requested_extensions.size());
  for (auto const& ext : requested_extensions) {
    const bool extension_is_present = std::find_if(
      extensions.begin(), extensions.end(),
      [ext](vk::ExtensionProperties const& ep) {
        return ext == ep.extensionName;
      }) != extensions.end();

    if (!extension_is_present) {
      fprintf(stderr, "Could not find Vulkan instance extension: %s\n", ext.c_str());
      continue;
    }

    enabled_extensions.push_back(ext.data());
  }

  return {enabled_layers, enabled_extensions};
}

#ifndef NDEBUG
VkBool32 DebugUtilsMessengerCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
  VkDebugUtilsMessageTypeFlagsEXT message_types,
  VkDebugUtilsMessengerCallbackDataEXT const *p_callback_data, void *) {
  std::cerr << vk::to_string(
    static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(message_severity))
            << ": " << vk::to_string(
              static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(message_types)) << ":\n";
  std::cerr << "\t" << "messageIDName   = <"
            << p_callback_data->pMessageIdName << ">\n";
  std::cerr << "\t" << "messageIdNumber = "
            << p_callback_data->messageIdNumber << "\n";
  std::cerr << "\t" << "message         = <"
            << p_callback_data->pMessage << ">\n";

  if (0 < p_callback_data->queueLabelCount) {
    std::cerr << "\t" << "Queue Labels:\n";
    for (uint8_t i = 0; i < p_callback_data->queueLabelCount; i++) {
      std::cerr << "\t\t" << "labelName = <"
                << p_callback_data->pQueueLabels[i].pLabelName << ">\n";
    }
  }

  if (0 < p_callback_data->cmdBufLabelCount) {
    std::cerr << "\t" << "CommandBuffer Labels:\n";
    for (uint8_t i = 0; i < p_callback_data->cmdBufLabelCount; i++) {
      std::cerr << "\t\t" << "labelName = <"
                << p_callback_data->pCmdBufLabels[i].pLabelName << ">\n";
    }
  }
  if (0 < p_callback_data->objectCount) {
    std::cerr << "\t" << "Objects:\n";
    for (uint8_t i = 0; i < p_callback_data->objectCount; i++) {
      std::cerr << "\t\t" << "Object "
                << i << "\n";
      std::cerr << "\t\t\t" << "objectType   = "
                << vk::to_string(
                  static_cast<vk::ObjectType>(p_callback_data->pObjects[i].objectType)) << "\n";
      std::cerr << "\t\t\t" << "objectHandle = "
                << p_callback_data->pObjects[i].objectHandle << "\n";
      if (p_callback_data->pObjects[i].pObjectName) {
        std::cerr << "\t\t\t" << "objectName   = <"
                  << p_callback_data->pObjects[i].pObjectName << ">\n";
      }
    }
  }
  return VK_TRUE;
}
#endif


namespace space {
  namespace core {
    std::optional<VkAppContext> InitVulkan(
      const VkAppConfig config, Display *display, Window window) {

      // Setup dynamic dispatcher
      vk::DynamicLoader dl;
      PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
      VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

      auto requested_layers = config.instance_layers;
      auto requested_extensions = config.instance_extensions;

#ifndef NDEBUG
      fprintf(stdout, "Enabling debug layers and extensions...\n");
      requested_layers.push_back("VK_LAYER_KHRONOS_validation");
      requested_layers.push_back("VK_LAYER_LUNARG_standard_validation");
      requested_layers.push_back("VK_LAYER_LUNARG_assistant_layer");
      requested_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

      auto le = GetInstanceEnabledLayersAndExtensions(requested_layers, requested_extensions);

      // Copy in a vector of const chars*.
      std::vector<const char*> enabled_instance_layers;
      std::vector<const char*> enabled_instance_extensions;
      for (auto &l : le.first)
        enabled_instance_layers.push_back(l.c_str());
      for (auto &e : le.second)
        enabled_instance_extensions.push_back(e.c_str());

      vk::ApplicationInfo application_info(
        config.app_name, 1, config.engine_name, 1, VK_API_VERSION_1_1);

      vk::InstanceCreateInfo instance_create_info(
        {}, &application_info, checked_cast<uint32_t>(enabled_instance_layers.size()),
        enabled_instance_layers.data(),
        checked_cast<uint32_t>(enabled_instance_extensions.size()),
        enabled_instance_extensions.data());

      vk::UniqueInstance instance = vk::createInstanceUnique(instance_create_info);
      VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

      vk::UniqueDebugUtilsMessengerEXT debug_utils_messenger;

#ifndef NDEBUG
      vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
      vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

      debug_utils_messenger =
        instance->createDebugUtilsMessengerEXTUnique(
          vk::DebugUtilsMessengerCreateInfoEXT(
            {}, severityFlags, messageTypeFlags,
            &DebugUtilsMessengerCallback), nullptr);
#endif

      // For now we just pick the first device. We should improve the code by
      // searching for the device that has best properties or any other policy
      // possibly configurable policy.
      vk::PhysicalDevice physical_device = instance->enumeratePhysicalDevices().front();

      // Init the surface
      vk::UniqueSurfaceKHR surface =
        instance->createXlibSurfaceKHRUnique(
          vk::XlibSurfaceCreateInfoKHR(vk::XlibSurfaceCreateFlagsKHR(), display, window));

      // Find devices for present and graphics.
      std::pair<uint32_t, uint32_t> graphics_and_present_queue_family_index;
      if (const auto o = FindGraphicsAndPresentQueueFamilyIndex(
            physical_device, *surface)) {
        graphics_and_present_queue_family_index = o.value();
      } else {
        fprintf(stderr, "Couldn't find suitable Present or Graphics queues.");
        return {};
      }

      // Create logical device. This can enable another set of extensions.
      const std::vector<std::string> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
      };

      const auto physical_device_features = physical_device.getFeatures();
      vk::UniqueDevice device = CreateDevice(
        physical_device, graphics_and_present_queue_family_index.first,
        device_extensions, &physical_device_features);
      VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

      return VkAppContext{
        std::move(dl),
        std::move(instance),
        std::move(surface),
        std::move(device),
        std::move(debug_utils_messenger),
        physical_device,
        graphics_and_present_queue_family_index.first,
        graphics_and_present_queue_family_index.second};
    }

    vk::SampleCountFlagBits GetMaxUsableSampleCount(const vk::PhysicalDevice &physical_device) {
      return vk::SampleCountFlagBits::e4;
      vk::PhysicalDeviceProperties properties = physical_device.getProperties();
      vk::SampleCountFlags counts = std::min(
        properties.limits.framebufferColorSampleCounts, properties.limits.framebufferDepthSampleCounts);

      if (counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
      if (counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e64;
      if (counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e64;
      if (counts & vk::SampleCountFlagBits::e8) return vk::SampleCountFlagBits::e64;
      if (counts & vk::SampleCountFlagBits::e4) return vk::SampleCountFlagBits::e64;
      if (counts & vk::SampleCountFlagBits::e2) return vk::SampleCountFlagBits::e64;
      return vk::SampleCountFlagBits::e1;
    }
  }
}
