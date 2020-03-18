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
// This file contains the represents an intermediate interface simplify vulkan.

#ifndef __SPACE_CORE_H_
#define __SPACE_CORE_H_

#include <optional>

#include <vulkan/vulkan.hpp>
#include <X11/Xlib.h>

template<class T>
inline constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

template <typename TargetType, typename SourceType>
VULKAN_HPP_INLINE TargetType checked_cast(SourceType value) {
  static_assert(
    sizeof(TargetType) <= sizeof(SourceType),
    "No need to cast from smaller to larger type!");
  static_assert(
    !std::numeric_limits<TargetType>::is_signed,
    "Only unsigned types supported!");
  static_assert(
    !std::numeric_limits<SourceType>::is_signed,
    "Only unsigned types supported!");
  assert(value <= std::numeric_limits<TargetType>::max());
  return static_cast<TargetType>(value);
}

namespace space {
  namespace core {
    // Vulkan initialization routines
    struct SurfaceData {
      vk::UniqueSurfaceKHR surface;
      vk::Extent2D extent;
    };

    struct VkAppConfig {
      const char *app_name;
      const char *engine_name;
      std::vector<std::string> instance_layers;
      std::vector<std::string> instance_extensions;
    };

    struct VkAppContext {
      vk::UniqueInstance instance;
      vk::PhysicalDevice physical_device;
      SurfaceData surface_data;
      uint32_t graphics_queue_family_index;
      uint32_t present_queue_family_index;
      vk::UniqueDevice device;
    };

    // Takes care of the super boring Vulkan bootstraping.
    std::optional<struct VkAppContext> InitVulkan(
      const VkAppConfig config, Display *display, Window window);

    // Vulkan rendering helper routines
    struct SwapChainData {
      SwapChainData(
        vk::PhysicalDevice const& physical_device,
        vk::UniqueDevice const& device,
        vk::SurfaceKHR const& surface, vk::Extent2D const& extent,
        vk::ImageUsageFlags usage,
        vk::UniqueSwapchainKHR const& old_swap_chain,
        uint32_t graphics_family_index,
        uint32_t present_family_index);
      vk::Format color_format;
      vk::UniqueSwapchainKHR swap_chain;
      std::vector<vk::Image> images;
      std::vector<vk::UniqueImageView> image_views;
    };

    class BufferData {
    public:
      BufferData(
        vk::PhysicalDevice const& physicalDevice,
        vk::UniqueDevice const& device, vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible
        | vk::MemoryPropertyFlagBits::eHostCoherent);

      template <typename DataType>
      void Upload(
        vk::UniqueDevice const& device, DataType const& data) const;

      template <typename DataType>
      void Upload(
        vk::UniqueDevice const& device, std::vector<DataType> const& data,
        size_t stride = 0) const;

      template <typename DataType>
      void Upload(
        vk::PhysicalDevice const& physicalDevice, vk::UniqueDevice const& device,
        vk::UniqueCommandPool const& commandPool, vk::Queue queue,
        std::vector<DataType> const& data,
        size_t stride) const;

      vk::UniqueBuffer buffer;
      vk::UniqueDeviceMemory deviceMemory;

    private:
      // For debugging pourposes only
      // We should optionally remove these checks.
      // For instance, is there enough space?
      vk::DeviceSize m_size;
      vk::BufferUsageFlags m_usage;
      vk::MemoryPropertyFlags m_propertyFlags;
    };

    struct ImageData {
      ImageData(vk::PhysicalDevice const& physical_device, vk::UniqueDevice const& device,
                vk::Format format, vk::Extent2D const& extent, vk::ImageTiling tiling,
                vk::ImageUsageFlags usage, vk::ImageLayout initial_layout,
                vk::MemoryPropertyFlags memory_properties, vk::ImageAspectFlags aspect_mask);
      vk::Format format;
      vk::UniqueImage image;
      vk::UniqueDeviceMemory device_memory;
      vk::UniqueImageView image_view;
    };

    struct DepthBufferData : public ImageData {
      DepthBufferData(
        vk::PhysicalDevice &physical_device, vk::UniqueDevice & device,
        vk::Format format, vk::Extent2D const& extent)
        : ImageData(
          physical_device, device, format, extent, vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageLayout::eUndefined,
          vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eDepth) {}
    };

    vk::UniqueCommandPool CreateCommandPool(vk::UniqueDevice &device, uint32_t queue_family_index);

    template <class T>
    void CopyToDevice(
      vk::UniqueDevice const& device, vk::UniqueDeviceMemory const& memory,
      T const* pData, size_t count, size_t stride = sizeof(T)) {
      assert(sizeof(T) <= stride);
      uint8_t* deviceData = static_cast<uint8_t*>(
        device->mapMemory(memory.get(), 0, count * stride));
      if (stride == sizeof(T)) {
        memcpy(deviceData, pData, count * sizeof(T));
      } else {
        for (size_t i = 0; i < count; i++) {
          memcpy(deviceData, &pData[i], sizeof(T));
          deviceData += stride;
        }
      }
      device->unmapMemory(memory.get());
    }

    template <class T>
    void CopyToDevice(
      vk::UniqueDevice const& device, vk::UniqueDeviceMemory const& memory,
      T const& data) { CopyToDevice<T>(device, memory, &data, 1); }

    template <typename Func>
    void OneTimeSubmit(
      vk::UniqueCommandBuffer const& commandBuffer, vk::Queue const& queue,
      Func const& func) {
      commandBuffer->begin(
        vk::CommandBufferBeginInfo(
          vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
      func(commandBuffer);
      commandBuffer->end();
      queue.submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &(*commandBuffer)), nullptr);
      queue.waitIdle();
    }

    template <typename Func>
    void OneTimeSubmit(
      vk::UniqueDevice const& device, vk::UniqueCommandPool const& commandPool,
      vk::Queue const& queue, Func const& func) {
      vk::UniqueCommandBuffer commandBuffer =
        std::move(device->allocateCommandBuffersUnique(
                    vk::CommandBufferAllocateInfo(
                      *commandPool, vk::CommandBufferLevel::ePrimary, 1)).front());
      OneTimeSubmit(commandBuffer, queue, func);
    }

    vk::UniqueDeviceMemory AllocateMemory(
      vk::UniqueDevice const& device,
      vk::PhysicalDeviceMemoryProperties const& memoryProperties,
      vk::MemoryRequirements const& memoryRequirements,
      vk::MemoryPropertyFlags memoryPropertyFlags);

    uint32_t FindMemoryType(
      vk::PhysicalDeviceMemoryProperties const& memoryProperties,
      uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask);

    vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(
      vk::UniqueDevice const& device,
      std::vector<std::tuple<vk::DescriptorType, uint32_t, vk::ShaderStageFlags>> const& bindingData,
      vk::DescriptorSetLayoutCreateFlags flags = {});

    void UpdateDescriptorSets(
      vk::UniqueDevice const& device, vk::UniqueDescriptorSet const& descriptorSet,
      std::vector<std::tuple<vk::DescriptorType,
      vk::UniqueBuffer const&,
      vk::UniqueBufferView const&>> const& bufferData, uint32_t bindingOffset = 0);
  }
}

#endif // __VULKAN-CORE_H_
