//
//  image.h
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

/** VkImage represents multidimensional data in the swapchain. They can be
 *    color/depth/stencil attachements, textures, etc. The exact purpose
 *    is not specified until we create an image view. For texture images, we use
 *    staging buffers to transfer data to the actual storage. During this
 *    process, the image layout wil change from UNDEFINED (since it may have
 *    very different layouts when it is on host, which makes it unusable for
 *    device) to TRANSFER_DST_OPTIMAL (so that we can transfer data from the
 *    staging buffer to it), and eventually to SHADER_READ_ONLY_OPTIMAL (optimal
 *    for device access).
 *
 *  Initialization:
 *    VkDevice
 *    VkSwapchainKHR
 *
 *------------------------------------------------------------------------------
 *
 *  VkImageView determines how to access and what part of images to access.
 *    We might convert the image format on the fly with it.
 *
 *  Initialization:
 *    VkDevice
 *    Image referenced by it
 *    View type (1D, 2D, 3D, cube, etc.)
 *    Format of the image
 *    Whether and how to remap RGBA channels
 *    Purpose of the image (color, depth, stencil, etc)
 *    Set of mipmap levels and array layers to be accessible
 *
 *------------------------------------------------------------------------------
 *
 *  VkSampler configures how do we sample and filter images.
 *
 *  Initialization:
 *    Minification/magnification filter mode (either nearest or linear)
 *    Mipmap mode (either nearest or linear)
 *    Address mode for u/v/w ([mirror] repeat, [mirror] clamp to edge/border)
 *    Mipmap setting (bias/min/max/compare op)
 *    Anisotropy filter setting (enable or not/max amount of samples)
 *    Border color
 *    Use image coordinates or normalized coordianates
 */

class SwapChainImage {
 public:
  SwapChainImage() = default;

  // This class is neither copyable nor movable
  SwapChainImage(const SwapChainImage&) = delete;
  SwapChainImage& operator=(const SwapChainImage&) = delete;

  ~SwapChainImage();

  void Init(const std::shared_ptr<Context>& context,
            const VkImage& image,
            VkFormat format);

  const VkImageView& image_view() const { return image_view_; }

 private:
  std::shared_ptr<Context> context_;
  VkImageView image_view_;
};

class TextureImage;
using SharedTexture = std::shared_ptr<const TextureImage>;

class TextureImage : public std::enable_shared_from_this<TextureImage> {
 public:
  // Textures will be put in a unified resource pool. For single images, its
  // file path will be used as identifier; for cubemaps, its directory will be
  // used as identifier.
  struct CubemapPath {
    enum Order { kPosX = 0, kNegX, kPosY, kNegY, kPosZ, kNegZ };
    std::string directory;
    std::array<std::string, buffer::kCubemapImageCount> files;
  };
  using SourcePath = absl::variant<std::string, CubemapPath>;

  static SharedTexture GetTexture(const std::shared_ptr<Context>& context,
                                  const SourcePath& source_path);

  // This class is neither copyable nor movable
  TextureImage(const TextureImage&) = delete;
  TextureImage& operator=(const TextureImage&) = delete;

  ~TextureImage();

  VkDescriptorImageInfo descriptor_info() const;

 private:
  TextureImage(const std::shared_ptr<Context>& context,
               const SourcePath& source_path);

  static absl::node_hash_map<std::string, SharedTexture> kLoadedTextures;
  std::shared_ptr<Context> context_;
  std::string identifier_;
  TextureBuffer buffer_;
  VkImageView image_view_;
  VkSampler sampler_;
};

class DepthStencilImage {
 public:
  DepthStencilImage() = default;

  // This class is neither copyable nor movable
  DepthStencilImage(const DepthStencilImage&) = delete;
  DepthStencilImage& operator=(const DepthStencilImage&) = delete;

  ~DepthStencilImage() { Cleanup(); }

  void Init(const std::shared_ptr<Context>& context,
            VkExtent2D extent);
  void Cleanup();

  VkFormat format()               const { return buffer_.format(); }
  const VkImageView& image_view() const { return image_view_; }

 private:
  std::shared_ptr<Context> context_;
  DepthStencilBuffer buffer_;
  VkImageView image_view_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_IMAGE_H */
