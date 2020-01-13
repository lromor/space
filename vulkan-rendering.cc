#include "vulkan-core.h"

static std::optional<vk::PresentModeKHR> PickPresentMode(
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

static std::optional<vk::SurfaceFormatKHR> PickSurfaceFormat(
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
      auto it = std::find_if(
        formats.begin(), formats.end(),
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

namespace vk {
  namespace core {
    SwapChainData::SwapChainData(
      vk::PhysicalDevice const& physical_device, vk::UniqueDevice const& device,
      vk::SurfaceKHR const& surface, vk::Extent2D const& extent, vk::ImageUsageFlags usage,
      vk::UniqueSwapchainKHR const& old_swap_chain, uint32_t graphics_queue_family_index,
      uint32_t present_queue_family_index) {

      vk::SurfaceFormatKHR surface_format =PickSurfaceFormat(
        physical_device.getSurfaceFormatsKHR(surface)).value();
      color_format = surface_format.format;

      vk::SurfaceCapabilitiesKHR surface_capabilities =
        physical_device.getSurfaceCapabilitiesKHR(surface);
      VkExtent2D swap_chain_extent;
      if (surface_capabilities.currentExtent.width
          == std::numeric_limits<uint32_t>::max()) {
        // If the surface size is undefined, the size is set to the size of the images requested.
        swap_chain_extent.width = clamp(
          extent.width, surface_capabilities.minImageExtent.width,
          surface_capabilities.maxImageExtent.width);
        swap_chain_extent.height = clamp(
          extent.height, surface_capabilities.minImageExtent.height,
          surface_capabilities.maxImageExtent.height);
      } else {
        // If the surface size is defined, the swap chain size must match
        swap_chain_extent = surface_capabilities.currentExtent;
      }
      vk::SurfaceTransformFlagBitsKHR pre_transform =
        (surface_capabilities.supportedTransforms
         & vk::SurfaceTransformFlagBitsKHR::eIdentity)
        ? vk::SurfaceTransformFlagBitsKHR::eIdentity
        : surface_capabilities.currentTransform;
      vk::CompositeAlphaFlagBitsKHR composite_alpha =
        (surface_capabilities.supportedCompositeAlpha
         & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
        ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
        : (surface_capabilities.supportedCompositeAlpha
           & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
        ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
        : (surface_capabilities.supportedCompositeAlpha
           & vk::CompositeAlphaFlagBitsKHR::eInherit)
        ? vk::CompositeAlphaFlagBitsKHR::eInherit
        : vk::CompositeAlphaFlagBitsKHR::eOpaque;
      vk::PresentModeKHR present_mode = PickPresentMode(
        physical_device.getSurfacePresentModesKHR(surface)).value();
      vk::SwapchainCreateInfoKHR swapChainCreateInfo(
        {}, surface, surface_capabilities.minImageCount,
        color_format, surface_format.colorSpace, swap_chain_extent,
        1, usage, vk::SharingMode::eExclusive, 0, nullptr,
        pre_transform, composite_alpha, present_mode, true, *old_swap_chain);
      if (graphics_queue_family_index != present_queue_family_index) {
        uint32_t queueFamilyIndices[2] =
          { graphics_queue_family_index, present_queue_family_index };
        // If the graphics and present queues are from different queue families, we either have to
        // explicitly transfer ownership of images between the queues, or we have to create the
        // swapchain with imageSharingMode as vk::SharingMode::eConcurrent
        swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
      }
      swap_chain = device->createSwapchainKHRUnique(swapChainCreateInfo);

      images = device->getSwapchainImagesKHR(swap_chain.get());

      image_views.reserve(images.size());
      vk::ComponentMapping component_mapping(
        vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
        vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
      vk::ImageSubresourceRange sub_resource_range(
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
      for (auto image : images) {
        vk::ImageViewCreateInfo image_view_create_info(
          vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, color_format,
          component_mapping, sub_resource_range);
        image_views.push_back(device->createImageViewUnique(image_view_create_info));
      }
    }

    vk::UniqueCommandPool CreateCommandPool(
      vk::UniqueDevice &device, uint32_t queue_family_index) {
      vk::CommandPoolCreateInfo commandPoolCreateInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        queue_family_index);
      return device->createCommandPoolUnique(commandPoolCreateInfo);
    }


    BufferData::BufferData(
      vk::PhysicalDevice const& physicalDevice, vk::UniqueDevice const& device,
      vk::DeviceSize size, vk::BufferUsageFlags usage,
      vk::MemoryPropertyFlags propertyFlags)
      : m_size(size), m_usage(usage), m_propertyFlags(propertyFlags) {
      buffer = device->createBufferUnique(
        vk::BufferCreateInfo(vk::BufferCreateFlags(), size, usage));
      deviceMemory = AllocateMemory(
        device, physicalDevice.getMemoryProperties(),
        device->getBufferMemoryRequirements(buffer.get()), propertyFlags);
      device->bindBufferMemory(buffer.get(), deviceMemory.get(), 0);
    }

    template <typename DataType>
    void BufferData::Upload(
      vk::UniqueDevice const& device, DataType const& data) const {
      assert((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)
             && (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
      assert(sizeof(DataType) <= m_size);

      void* dataPtr = device->mapMemory(*this->deviceMemory, 0, sizeof(DataType));
      memcpy(dataPtr, &data, sizeof(DataType));
      device->unmapMemory(*this->deviceMemory);
    }

    template <typename DataType>
    void BufferData::Upload(
      vk::UniqueDevice const& device, std::vector<DataType> const& data,
      size_t stride) const {
      assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

      size_t elementSize = stride ? stride : sizeof(DataType);
      assert(sizeof(DataType) <= elementSize);

      CopyToDevice(device, deviceMemory, data.data(), data.size(), elementSize);
    }

    template <typename DataType>
    void BufferData::Upload(
      vk::PhysicalDevice const& physicalDevice, vk::UniqueDevice const& device,
      vk::UniqueCommandPool const& commandPool, vk::Queue queue,
      std::vector<DataType> const& data, size_t stride) const {
      assert(m_usage & vk::BufferUsageFlagBits::eTransferDst);
      assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);

      size_t elementSize = stride ? stride : sizeof(DataType);
      assert(sizeof(DataType) <= elementSize);

      size_t dataSize = data.size() * elementSize;
      assert(dataSize <= m_size);

      BufferData stagingBuffer(
        physicalDevice, device, dataSize, vk::BufferUsageFlagBits::eTransferSrc);

      CopyToDevice(
        device, stagingBuffer.deviceMemory, data.data(),
        data.size(), elementSize);

      OneTimeSubmit(device, commandPool, queue,
                    [&](vk::UniqueCommandBuffer const& commandBuffer) {
                      commandBuffer->copyBuffer(*stagingBuffer.buffer, *this->buffer, vk::BufferCopy(0, 0, dataSize));
                    });
    }

    template <class T>
    void CopyToDevice(
      vk::UniqueDevice const& device, vk::UniqueDeviceMemory const& memory,
      T const* pData, size_t count, size_t stride) {
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
      T const& data) {
      CopyToDevice<T>(device, memory, &data, 1);
    }

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
      vk::MemoryPropertyFlags memoryPropertyFlags) {
      uint32_t memoryTypeIndex = FindMemoryType(
        memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);
      return device->allocateMemoryUnique(
        vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));
    }

    uint32_t FindMemoryType(
      vk::PhysicalDeviceMemoryProperties const& memoryProperties,
      uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask) {
      uint32_t typeIndex = uint32_t(~0);
      for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)) {
          typeIndex = i;
          break;
        }
        typeBits >>= 1;
      }
      assert(typeIndex != ~0u);
      return typeIndex;
    }
  }
}
