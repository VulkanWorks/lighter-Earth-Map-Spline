//
//  command.h
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_COMMAND_H
#define WRAPPER_VULKAN_COMMAND_H

#include <functional>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "basic_object.h"
#include "buffer.h"
#include "pipeline.h"
#include "synchronize.h"

namespace wrapper {
namespace vulkan {

class Context;

/** VkCommandPool allocates command buffer memory.
 *
 *  Initialization:
 *      Queue family index
 *
 *------------------------------------------------------------------------------
 *
 *  VkCommandBuffer records all operations we want to perform and submit to a
 *      device queue for execution. Primary level command buffers can call
 *      secondary level ones and submit to queues, while secondary levels ones
 *      are not directly submitted.
 *
 *  Initialization:
 *      VkCommandPool
 *      Level (either primary or secondary)
 */
class Command {
 public:
  static const size_t kMaxFrameInFlight{2};

  using RecordCommand = std::function<void (const VkCommandBuffer&)>;
  static void OneTimeCommand(
      const VkDevice& device,
      const Queues::Queue& queue,
      const VkAllocationCallbacks* allocator,
      const RecordCommand& on_record);

  Command() = default;
  VkResult DrawFrame(const UniformBuffer& uniform_buffer,
                     const std::function<void (size_t)>& update_func);
  void Init(std::shared_ptr<Context> context,
            const Pipeline& pipeline,
            const VertexBuffer& vertex_buffer,
            const UniformBuffer& uniform_buffer);
  void Cleanup();
  ~Command();

  // This class is neither copyable nor movable
  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

  size_t current_frame() const { return current_frame_; }

 private:
  std::shared_ptr<Context> context_;
  size_t current_frame_{0};
  bool is_first_time_{true};
  Semaphores image_available_semas_;
  Semaphores render_finished_semas_;
  Fences in_flight_fences_;
  VkCommandPool command_pool_;
  std::vector<VkCommandBuffer> command_buffers_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_COMMAND_H */
