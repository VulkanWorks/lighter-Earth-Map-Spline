//
//  renderpass.hpp
//  LearnVulkan
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef renderpass_hpp
#define renderpass_hpp

#include <vector>

#include <vulkan/vulkan.hpp>

namespace VulkanWrappers {
    using std::vector;
    
    class Application;
    
    /** VkRenderPass specifies how to use color and depth buffers.
     *
     *  Initialization:
     *      VkDevice
     *      List of VkAttachmentDescription
     *      List of VkSubpassDescription
     *      List of VkSubpassDependency
     *
     *--------------------------------------------------------------------------
     *
     *  VkFramebuffer associates each VkImageView with an attachment.
     *
     *  Initialization:
     *      VkRenderPass
     *      List of VkImageView
     *      Image extent (width, height and number of layers)
     */
    class RenderPass {
        const Application &app;
        VkRenderPass renderPass;
        vector<VkFramebuffer> framebuffers;
        
    public:
        RenderPass(const Application &app) : app{app} {}
        void init();
        void cleanup();
        ~RenderPass();
        
        const VkRenderPass &operator*(void)             const { return renderPass; }
        const vector<VkFramebuffer> &getFramebuffers()  const { return framebuffers; }
    };
}

#endif /* renderpass_hpp */
