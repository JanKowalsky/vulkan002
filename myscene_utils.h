#ifndef MYSCENE_UTILS_H
#define MYSCENE_UTILS_H

#include <vulkan/vulkan.h>
#include "vulkan_math.h"
#include <vector>

struct Vertex
{
    glm::vec3 pos;
    float p0;
    glm::vec3 norm;
    float p1;
    glm::vec2 uv;
    float p2[2];
};

struct VulkanImage
{
	void destroy();
	
	VkImage img;
	VkImageView img_view;
	VkDeviceMemory mem;
};

struct RenderTarget
{
	void destroy();
	
	VulkanImage depth_stencil_buffer;
	VulkanImage target_image;
	VkRenderPass render_pass;
	VkFramebuffer framebuffer;
	VkRenderPassBeginInfo begin_info;
	std::vector<VkClearValue> clear_values;
};

#endif //MYSCENE_UTILS_H
