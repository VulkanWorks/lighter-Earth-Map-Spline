//
//  pipeline.h
//
//  Created by Pujun Lun on 2/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Forward declarations.
class Pipeline;

// The user should use this class to create Pipeline. The internal states,
// except for shader modules, are preserved when it is used to build a pipeline,
// so the user can add shaders again and reuse the builder later.
class PipelineBuilder {
 public:
  // TODO: Similar to texture image, make a shader file pool.
  using ShaderInfo = std::pair</*shader_stage*/VkShaderStageFlagBits,
                               /*file_path*/std::string>;

  using ShaderModule = std::pair</*shader_stage*/VkShaderStageFlagBits,
                                 /*shader_module*/VkShaderModule>;

  using ViewportInfo = std::pair</*viewport*/VkViewport, /*scissor*/VkRect2D>;

  // Internal states will be filled with default settings, unless they are of
  // absl::optional or std::vector types.
  explicit PipelineBuilder(SharedBasicContext context);

  // This class is neither copyable nor movable.
  PipelineBuilder(const PipelineBuilder&) = delete;
  PipelineBuilder& operator=(const PipelineBuilder&) = delete;

  // By default, depth testing and stencil testing are disabled, front face
  // direction is counter-clockwise, and the rasterizer only takes one sample.
  PipelineBuilder& EnableDepthTest();
  PipelineBuilder& EnableStencilTest();
  PipelineBuilder& SetFrontFaceClockwise();
  PipelineBuilder& SetMultisampling(VkSampleCountFlagBits sample_count);

  // Sets vertex input bindings and attributes. The user may use helper
  // functions in pipeline_util to construct input arguments.
  PipelineBuilder& SetVertexInput(
      std::vector<VkVertexInputBindingDescription>&& binding_descriptions,
      std::vector<VkVertexInputAttributeDescription>&& attribute_descriptions);

  // Sets descriptor set layouts and push constant ranges used in this pipeline.
  // Note that according to the Vulkan specification, to be compatible with all
  // devices, we only allow the user to push constants of at most 128 bytes in
  // total within one pipeline.
  PipelineBuilder& SetPipelineLayout(
      std::vector<VkDescriptorSetLayout>&& descriptor_layouts,
      std::vector<VkPushConstantRange>&& push_constant_ranges);

  // Sets the viewport and scissor.
  PipelineBuilder& SetViewport(ViewportInfo&& info);

  // Specifies that this pipeline will be used in the subpass of 'render_pass'
  // with 'subpass_index'.
  PipelineBuilder& SetRenderPass(const VkRenderPass& render_pass,
                                 uint32_t subpass_index);

  // Sets color blend states for each color attachment. The length of
  // 'color_blend_states' must match the number of color attachments used in
  // the subpass.
  PipelineBuilder& SetColorBlend(
      std::vector<VkPipelineColorBlendAttachmentState>&& color_blend_states);

  // Adds a shader to the pipeline. Note that after Build() is called, the user
  // should add all shaders again before building another pipeline.
  PipelineBuilder& AddShader(const ShaderInfo& info);

  // Returns a pipeline. This can be called multiple times. Note that after one
  // call, the user should add all shaders again before another call.
  std::unique_ptr<Pipeline> Build();

 private:
  using RenderPassInfo = std::pair</*render_pass*/VkRenderPass,
                                   /*subpass_index*/uint32_t>;

  // Pointer to context.
  const SharedBasicContext context_;

  // Specifies how to assemble primitives.
  VkPipelineInputAssemblyStateCreateInfo input_assembly_info_;

  // Configures the rasterizer state, including the front face direction and
  // face culling mode.
  VkPipelineRasterizationStateCreateInfo rasterization_info_;

  // Configures the multisampling state.
  VkPipelineMultisampleStateCreateInfo multisampling_info_;

  // Specifies whether to enable depth and/or stencil testing.
  VkPipelineDepthStencilStateCreateInfo depth_stencil_info_;

  // States that can be modified without recreating the entire pipeline.
  VkPipelineDynamicStateCreateInfo dynamic_state_info_;

  // Configures vertex input bindings and attributes.
  absl::optional<VkPipelineVertexInputStateCreateInfo> vertex_input_info_;
  std::vector<VkVertexInputBindingDescription> binding_descriptions_;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions_;

  // Descriptor sets and push constants determine the layout of the pipeline.
  absl::optional<VkPipelineLayoutCreateInfo> pipeline_layout_info_;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  std::vector<VkPushConstantRange> push_constant_ranges_;

  // Specifies the viewport and scissor.
  absl::optional<ViewportInfo> viewport_info_;

  // Specifies this pipeline will be used in which render pass and subpass.
  absl::optional<RenderPassInfo> render_pass_info_;

  // Color blend states of each color attachment.
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_states_;

  // Shaders used in the pipeline. This will be cleared after Build() is called.
  std::vector<ShaderModule> shader_modules_;
};

// VkPipeline configures multiple shader stages, multiple fixed function stages
// (including vertex input bindings and attributes, primitive assembly,
// tessellation, viewport and scissor, rasterization, multisampling, depth
// testing and stencil testing, color blending, and dynamic states), and the
// pipeline layout (including descriptor set layouts and push constant ranges).
// The user should use PipelineBuilder to create instances of this class.
// If any state is changed, for example, the render pass and viewport may change
// if the window is resized, the user should discard the old pipeline and build
// a new one with the updated states.
class Pipeline {
 public:
  // This class is neither copyable nor movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  ~Pipeline();

  // Binds to this pipeline. This should be called when 'command_buffer' is
  // recording commands.
  void Bind(const VkCommandBuffer& command_buffer) const;

  // Overloads.
  const VkPipeline& operator*() const { return pipeline_; }

  // Accessors.
  const VkPipelineLayout& layout() const { return layout_; }

 private:
  friend std::unique_ptr<Pipeline> PipelineBuilder::Build();

  Pipeline(SharedBasicContext context,
           const VkPipeline& pipeline,
           const VkPipelineLayout& pipeline_layout)
      : context_{std::move(context)},
        pipeline_{pipeline}, layout_{pipeline_layout} {}

  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque pipeline object.
  VkPipeline pipeline_;

  // Opaque pipeline layout object.
  VkPipelineLayout layout_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_H */
