//
//  buffer.h
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H

#include <memory>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace buffer {

constexpr int kCubemapImageCount = 6;
constexpr uint32_t kPerVertexBindingPoint = 0;
constexpr uint32_t kPerInstanceBindingPoint = 1;

struct CopyInfo {
  const void* data;
  VkDeviceSize size;
  VkDeviceSize offset;
};

} /* namespace buffer */

class Context;

/** VkBuffer represents linear arrays of data and configures usage of the data.
 *    Data can be transferred between buffers with the help of transfer queues.
 *    For buffers that contain large amount of data and do not change very
 *    often, we will create a staging buffer (which is visible to both host and
 *    device, and thus is not the most efficient for device) and a final buffer
 *    (which is only visible to device, and thus is optimal for device access).
 *    The staging buffer will only be used to transfer data to the final buffer,
 *    and then it will be destroyed.
 *
 *  Initialization:
 *    VkDevice
 *    Data size
 *    Buffer usage (vertex/index buffer, src/dst of transfer)
 *    Shared by which device queues
 *
 *------------------------------------------------------------------------------
 *
 *  VkDeviceMemory is a handle to the actual data stored in device memory.
 *    When we transfer data from host to device, we interact with
 *    VkPhysicalDevice rather than VkBuffer.
 *
 *  Initialization:
 *    Data size (can be different from the size we allocate for the buffer
 *      because of alignments)
 *    Memory type (visibility to host and device)
 *    VkBuffer that will be bound with it
 *    VkDevice
 *    VkPhysicalDevice
 */
class VertexBuffer {
 public:
  virtual ~VertexBuffer();

 protected:
  void CopyHostData(std::vector<buffer::CopyInfo> copy_infos,
                    size_t total_size);

  std::shared_ptr<Context> context_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
};

class PerVertexBuffer : public VertexBuffer {
 public:
  struct Info {
    struct Field {
      const void* data;
      size_t data_size;
      uint32_t unit_count;
    };
    Field vertices, indices;
  };

  PerVertexBuffer() = default;

  // This class is neither copyable nor movable
  PerVertexBuffer(const PerVertexBuffer&) = delete;
  PerVertexBuffer& operator=(const PerVertexBuffer&) = delete;

  ~PerVertexBuffer() override = default;

  void Init(const std::shared_ptr<Context>& context,
            const std::vector<Info>& infos);
  void Draw(const VkCommandBuffer& command_buffer,
            size_t mesh_index,
            uint32_t instance_count) const;

 private:
  struct MeshData {
    VkDeviceSize vertices_offset;
    VkDeviceSize indices_offset;
    uint32_t indices_count;
  };
  std::vector<MeshData> mesh_datas_;
};

class PerInstanceBuffer : public VertexBuffer {
 public:
  PerInstanceBuffer() = default;

  // This class is neither copyable nor movable
  PerInstanceBuffer(const PerInstanceBuffer&) = delete;
  PerInstanceBuffer& operator=(const PerInstanceBuffer&) = delete;

  ~PerInstanceBuffer() override = default;

  void Init(const std::shared_ptr<Context>& context,
            const void* data,
            size_t data_size);
  void Bind(const VkCommandBuffer& command_buffer);
};

class UniformBuffer {
 public:
  struct Info {
    size_t chunk_size;
    size_t num_chunk;
  };

  UniformBuffer() = default;

  // This class is neither copyable nor movable
  UniformBuffer(const UniformBuffer&) = delete;
  UniformBuffer& operator=(const UniformBuffer&) = delete;

  ~UniformBuffer();

  void Init(const std::shared_ptr<Context>& context,
            const Info& info);
  void CopyToDevice(size_t chunk_index) const;

  template <typename DataType>
  DataType* data(size_t chunk_index) const {
    return reinterpret_cast<DataType*>(data_ + chunk_data_size_ * chunk_index);
  }

  VkDescriptorBufferInfo descriptor_info(size_t chunk_index) const;

 private:
  std::shared_ptr<Context> context_;
  char* data_;
  size_t chunk_memory_size_, chunk_data_size_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
};

class TextureBuffer {
 public:
  struct Info{
    VkExtent3D extent() const { return {width, height, /*depth=*/1}; }
    VkDeviceSize data_size() const {
      return datas.size() * width * height * channel;
    }

    absl::Span<const void*> datas;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t channel;
  };

  TextureBuffer() = default;

  // This class is neither copyable nor movable
  TextureBuffer(const TextureBuffer&) = delete;
  TextureBuffer& operator=(const TextureBuffer&) = delete;

  ~TextureBuffer();

  void Init(const std::shared_ptr<Context>& context,
            const Info& info);

  const VkImage& image() const { return image_; }

 private:
  std::shared_ptr<Context> context_;
  VkImage image_;
  VkDeviceMemory device_memory_;
};

class DepthStencilBuffer {
 public:
  DepthStencilBuffer() = default;

  // This class is neither copyable nor movable
  DepthStencilBuffer(const DepthStencilBuffer&) = delete;
  DepthStencilBuffer& operator=(const DepthStencilBuffer&) = delete;

  ~DepthStencilBuffer() { Cleanup(); }

  void Init(const std::shared_ptr<Context>& context,
            VkExtent2D extent);
  void Cleanup();

  const VkImage& image() const { return image_; }
  VkFormat format()      const { return format_; }

 private:
  std::shared_ptr<Context> context_;
  VkImage image_;
  VkDeviceMemory device_memory_;
  VkFormat format_;
};

struct PushConstant {
  PushConstant() = default;

  // This class is neither copyable nor movable.
  PushConstant(const PushConstant&) = delete;
  PushConstant& operator=(const PushConstant&) = delete;

  ~PushConstant() { delete[] raw_data; }

  void Init(size_t size) {
    this->size = static_cast<uint32_t>(size);
    this->raw_data = new char[size];
  }

  template <typename DataType>
  DataType* data() const { return reinterpret_cast<DataType*>(raw_data); }

  uint32_t size;
  char* raw_data;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H */
