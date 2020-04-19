//
//  util.h
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H

#include <cstdlib>
#include <type_traits>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/timer.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/image_util.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/absl/flags/flag.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/glm/gtc/matrix_transform.hpp"
#include "third_party/vulkan/vulkan.h"

ABSL_DECLARE_FLAG(bool, performance_mode);

namespace jessie_steamer {
namespace application {
namespace vulkan {

// This is the base class of all applications. Its constructor simply forwards
// all arguments to the constructor of WindowContext. Each application should
// overwrite MainLoop() to render custom scenes.
class Application {
 public:
  template <typename... Args>
  explicit Application(Args&&... args)
      : window_context_{std::forward<Args>(args)...} {}

  // This class is neither copyable nor movable.
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  // Main loop of the application.
  virtual void MainLoop() = 0;

 protected:
  // Accessors.
  const wrapper::vulkan::WindowContext& window_context() {
    return window_context_;
  }
  wrapper::vulkan::WindowContext* mutable_window_context() {
    return &window_context_;
  }
  wrapper::vulkan::SharedBasicContext context() const {
    return window_context_.basic_context();
  }

 private:
  // On-screen rendering context.
  wrapper::vulkan::WindowContext window_context_;
};

// Parses command line arguments, sets necessary environment variables,
// instantiates an application of AppType, and runs its MainLoop().
// AppType must be a subclass of Application. 'app_args' will be forwarded to
// the constructor of the application.
template <typename AppType, typename... AppArgs>
int AppMain(int argc, char* argv[], AppArgs&&... app_args) {
  static_assert(std::is_base_of<Application, AppType>::value,
                "Not a subclass of Application");

  common::util::ParseCommandLine(argc, argv);

  if (absl::GetFlag(FLAGS_performance_mode)) {
    // To avoid the frame rate being clamped on MacOS when using MoltenVK:
    // https://github.com/KhronosGroup/MoltenVK/issues/581#issuecomment-487293665
    setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", /*overwrite=*/1);
    setenv("MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", /*overwrite=*/1);
  }

  // Set up the path to find Vulkan SDK.
  using common::file::GetVulkanSdkPath;
  setenv("VK_ICD_FILENAMES",
         GetVulkanSdkPath("share/vulkan/icd.d/MoltenVK_icd.json").c_str(),
         /*overwrite=*/1);
#ifndef NDEBUG
  setenv("VK_LAYER_PATH",
         GetVulkanSdkPath("share/vulkan/explicit_layer.d").c_str(),
         /*overwrite=*/1);
#endif /* !NDEBUG */

  // We don't catch exceptions in the debug mode, so that if there is anything
  // wrong, the debugger would stay at the point where the application breaks.
#ifdef NDEBUG
  try {
#endif /* NDEBUG */
    AppType app{std::forward<AppArgs>(app_args)...};
    app.MainLoop();
#ifdef NDEBUG
  } catch (const std::exception& e) {
    LOG_ERROR << "Error: " << e.what();
    return EXIT_FAILURE;
  }
#endif /* NDEBUG */

  return EXIT_SUCCESS;
}

// This class is used for rendering the given image to full screen. It is mainly
// used for debugging.
class ImageViewer {
 public:
  ImageViewer(const wrapper::vulkan::SharedBasicContext& context,
              const wrapper::vulkan::SamplableImage& image,
              int num_channels, bool flip_y);

  // This class is neither copyable nor movable.
  ImageViewer(const ImageViewer&) = delete;
  ImageViewer& operator=(const ImageViewer&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(const VkExtent2D& frame_size,
                         const wrapper::vulkan::RenderPass& render_pass,
                         uint32_t subpass_index);

  // Renders the image.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer) const;

 private:
  // Objects used for rendering.
  std::unique_ptr<wrapper::vulkan::StaticDescriptor> descriptor_;
  std::unique_ptr<wrapper::vulkan::PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<wrapper::vulkan::GraphicsPipelineBuilder> pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H */
