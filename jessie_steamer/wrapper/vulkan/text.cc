//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

#include <cmath>
#include <algorithm>

#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::Vertex2D;
using std::string;
using std::vector;

constexpr uint32_t kVertexBufferBindingPoint = 0;

enum BindingPoint { kUniformBufferBindingPoint = 0, kTextureBindingPoint };

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct TextRenderInfo {
  ALIGN_VEC4 glm::vec4 color_alpha;
};

/* END: Consistent with uniform blocks defined in shaders. */

// Returns the starting horizontal offset.
float GetOffsetX(float base_x, Text::Align align, float total_width) {
  switch (align) {
    case Text::Align::kLeft:
      return base_x;
    case Text::Align::kCenter:
      return base_x - total_width / 2.0f;
    case Text::Align::kRight:
      return base_x - total_width;
  }
}

// Returns descriptor infos for rendering text.
const vector<Descriptor::Info>& GetDescriptorInfos() {
  static const vector<Descriptor::Info>* descriptor_infos = nullptr;
  if (descriptor_infos == nullptr) {
    descriptor_infos = new vector<Descriptor::Info>{
        Descriptor::Info{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            /*bindings=*/{
                Descriptor::Info::Binding{
                    kUniformBufferBindingPoint,
                    /*array_length=*/1,
                },
            },
        },
        Descriptor::Info{
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            /*bindings=*/{
                Descriptor::Info::Binding{
                    kTextureBindingPoint,
                    /*array_length=*/1,
                },
            },
        },
    };
  }
  return *descriptor_infos;
}

// Returns a copy of 'value', but removes the minus sign of 'value.x' if exists.
inline glm::vec2 SetXPositive(const glm::vec2& value) {
  return glm::vec2{std::abs(value.x), value.y};
}

} /* namespace */

Text::Text(const SharedBasicContext& context,
           string&& pipeline_name,
           int num_frames_in_flight,
           float viewport_aspect_ratio)
    : viewport_aspect_ratio_{viewport_aspect_ratio},
      vertex_buffer_{context, text_util::GetVertexDataSize(/*num_rects=*/1),
                     pipeline::GetVertexAttribute<Vertex2D>()},
      uniform_buffer_{context, sizeof(TextRenderInfo), num_frames_in_flight},
      pipeline_builder_{context} {
  pipeline_builder_
      .SetName(std::move(pipeline_name))
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      vertex_buffer_.GetAttributes(/*start_location=*/0))
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("char.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("text.frag"));
}

void Text::Update(const VkExtent2D& frame_size,
                  VkSampleCountFlagBits sample_count,
                  const RenderPass& render_pass, uint32_t subpass_index) {
  pipeline_ = pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(pipeline::GetViewport(frame_size, viewport_aspect_ratio_))
      .SetRenderPass(*render_pass, subpass_index)
      .SetColorBlend(
          vector<VkPipelineColorBlendAttachmentState>(
              render_pass.num_color_attachments(subpass_index),
              pipeline::GetColorBlendState(/*enable_blend=*/true)))
      .Build();
}

int Text::UpdateBuffers(int frame, const glm::vec3& color, float alpha) {
  uniform_buffer_.HostData<TextRenderInfo>(frame)->color_alpha =
      glm::vec4(color, alpha);
  uniform_buffer_.Flush(frame);

  constexpr int kNumVerticesPerMesh = text_util::kNumVerticesPerRect;
  const auto num_meshes = static_cast<int>(vertices_to_draw_.size()) /
                          kNumVerticesPerMesh;
  vertex_buffer_.CopyHostData(PerVertexBuffer::ShareIndicesDataInfo{
      num_meshes,
      /*per_mesh_vertices=*/{vertices_to_draw_, kNumVerticesPerMesh},
      /*shared_indices=*/
      {PerVertexBuffer::VertexDataInfo{text_util::GetIndicesPerRect()}},
  });
  vertices_to_draw_.clear();

  return num_meshes;
}

void Text::SetPipelineLayout(const VkDescriptorSetLayout& layout) {
  pipeline_builder_.SetPipelineLayout({layout}, /*push_constant_ranges=*/{});
}

VkDescriptorBufferInfo Text::GetUniformBufferDescriptorInfo(int frame) const {
  return uniform_buffer_.GetDescriptorInfo(frame);
}

StaticText::StaticText(const SharedBasicContext& context,
                       int num_frames_in_flight,
                       float viewport_aspect_ratio,
                       const vector<string>& texts,
                       Font font, int font_height)
    : Text{context, "static text", num_frames_in_flight, viewport_aspect_ratio},
      text_loader_{context, texts, font, font_height} {
  descriptors_.reserve(num_frames_in_flight);
  push_descriptors_.reserve(num_frames_in_flight);

  const auto& descriptor_infos = GetDescriptorInfos();
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    descriptors_.emplace_back(
        absl::make_unique<DynamicDescriptor>(context, descriptor_infos));
    push_descriptors_.emplace_back(
        [=](const VkCommandBuffer& command_buffer,
            const VkPipelineLayout& pipeline_layout,
            int text_index) {
          descriptors_[frame]->PushBufferInfos(
              command_buffer, pipeline_layout,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              /*buffer_info_map=*/{{
                  kUniformBufferBindingPoint,
                  {GetUniformBufferDescriptorInfo(frame)},
              }}
          );
          descriptors_[frame]->PushImageInfos(
              command_buffer, pipeline_layout,
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              /*image_info_map=*/{{
                  kTextureBindingPoint,
                  {text_loader_.texture_info(text_index).image
                       ->GetDescriptorInfo()},
              }}
          );
        });
  }
  SetPipelineLayout(descriptors_[0]->layout());
}

glm::vec2 StaticText::AddText(int text_index, float height, float base_x,
                              float base_y, Align align) {
  texts_to_draw_.emplace_back(text_index);
  const auto& texture_info = text_loader_.texture_info(text_index);
  // If 'height' is negative, we should avoid to negate X-axis of ratio.
  const glm::vec2 ratio = SetXPositive(
      glm::vec2{texture_info.aspect_ratio / viewport_aspect_ratio(), 1.0f} *
      (height / 1.0f));
  const float width_in_frame = 1.0f * ratio.x;
  const float offset_x = GetOffsetX(base_x, align, width_in_frame);
  text_util::AppendCharPosAndTexCoord(
      /*pos_bottom_left=*/{offset_x, base_y - texture_info.base_y * ratio.y},
      /*pos_increment=*/glm::vec2{1.0f} * ratio,
      /*tex_coord_bottom_left=*/glm::vec2{0.0f},
      /*tex_coord_increment=*/glm::vec2{1.0f},
      mutable_vertices());

  return glm::vec2{offset_x, offset_x + width_in_frame};
}

void StaticText::Draw(const VkCommandBuffer& command_buffer,
                      int frame, const glm::vec3& color, float alpha) {
  const int num_texts = UpdateBuffers(frame, color, alpha);
  ASSERT_TRUE(num_texts == texts_to_draw_.size(),
              absl::StrFormat("Expected number of texts: %d vs %d",
                              num_texts, texts_to_draw_.size()));
  pipeline().Bind(command_buffer);
  for (int i = 0; i < num_texts; ++i) {
    push_descriptors_[frame](command_buffer, pipeline().layout(),
                             texts_to_draw_[i]);
    vertex_buffer().Draw(command_buffer, kVertexBufferBindingPoint,
                         /*mesh_index=*/i, /*instance_count=*/1);
  }
  texts_to_draw_.clear();
}

DynamicText::DynamicText(const SharedBasicContext& context,
                         int num_frames_in_flight,
                         float viewport_aspect_ratio,
                         const vector<string>& texts,
                         Font font, int font_height)
    : Text{context, "dynamic text", num_frames_in_flight,
           viewport_aspect_ratio},
      char_loader_{context, texts, font, font_height} {
  const auto& descriptor_infos = GetDescriptorInfos();
  const Descriptor::ImageInfoMap image_info_map{{
      kTextureBindingPoint,
      {char_loader_.library_image()->GetDescriptorInfo()},
  }};

  descriptors_.reserve(num_frames_in_flight);
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    descriptors_.emplace_back(
        absl::make_unique<StaticDescriptor>(context, descriptor_infos));
    descriptors_[frame]->UpdateBufferInfos(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        /*buffer_info_map=*/{{
            kUniformBufferBindingPoint,
            {GetUniformBufferDescriptorInfo(frame)},
        }});
    descriptors_[frame]->UpdateImageInfos(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info_map);
  }
  SetPipelineLayout(descriptors_[0]->layout());
}

glm::vec2 DynamicText::AddText(const std::string& text, float height,
                               float base_x, float base_y, Align align) {
  // If 'height' is negative, we should avoid to negate X-axis of ratio.
  const glm::vec2 ratio = SetXPositive(
      glm::vec2{char_loader_.GetAspectRatio() / viewport_aspect_ratio(), 1.0f} *
      (height / 1.0f));
  float total_width_in_tex_coord = 0.0f;
  for (auto character : text) {
    if (character == ' ') {
      total_width_in_tex_coord += char_loader_.space_advance();
    } else {
      const auto& texture_info = char_loader_.char_texture_info(character);
      total_width_in_tex_coord += texture_info.advance_x;
    }
  }

  const float initial_offset_x = GetOffsetX(base_x, align,
                                            total_width_in_tex_coord * ratio.x);
  const float final_offset_x = text_util::LoadCharsVertexData(
      text, char_loader_, ratio, initial_offset_x, base_y, /*flip_y=*/false,
      mutable_vertices());

  return glm::vec2{initial_offset_x, final_offset_x};
}

void DynamicText::Draw(const VkCommandBuffer& command_buffer,
                       int frame, const glm::vec3& color, float alpha) {
  const int num_chars = UpdateBuffers(frame, color, alpha);
  pipeline().Bind(command_buffer);
  descriptors_[frame]->Bind(command_buffer, pipeline().layout());
  for (int i = 0; i < num_chars; ++i) {
    vertex_buffer().Draw(command_buffer, kVertexBufferBindingPoint,
                         /*mesh_index=*/i, /*instance_count=*/1);
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
