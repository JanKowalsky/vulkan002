#include "myscene.h"
#include "shader.h"
#include <iostream>
#include <cstdlib>
#include <future>
#include <QImage>

enum SemNames{s_acquire_image, s_submit, num_sems};
//enum FenNames{f_submit, num_fences};

constexpr const auto img_filename = "bridge.jpg";

constexpr const float x_bound = 1000.0f;
constexpr const float y_bound = 1000.0f;
constexpr const float z_bound = 1000.0f;
constexpr const float min_speed = 50.0f;
constexpr const float mid_speed = 100.0f;
constexpr const float top_speed = 200.0f;

constexpr const uint32_t video_res_x = 1920;
constexpr const uint32_t video_res_y = 1080;
constexpr const uint8_t video_fps = 25;
constexpr const auto vid_filename = "tmp.mp4";
bool recording = false;

bool snap = false;

bool cam_move = false;

struct s_constants
{
	glm::mat4x4 vp;
	float dt;
	float particle_speed = min_speed;
    uint32_t res_x = 400;
    uint32_t res_y = 300;
	glm::vec3 eyeW;
	float p0;
	glm::vec3 curDirNW;
	float p1;
} constants;

void loadImage(std::string pathname, uint16_t size_x, uint16_t size_y, void** dst)
{
    /*Load image from file*/
    QImage img(pathname.c_str());

    /*Resize it and convert to suitable format*/
    img = img.scaled(size_x, size_y, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    img = img.convertToFormat(QImage::Format_RGBA8888);

    /*Convert integer values <0;255> to floats <0;1>*/

    uint8_t* src_pix = img.bits();
    float* dst_pix = (float*)*dst;

    for(uint32_t p = 0; p < size_x * size_y; p++)
    {
        dst_pix[p*4] = static_cast<float>(src_pix[p*4]) / 255.0f;
        dst_pix[p*4 + 1] = static_cast<float>(src_pix[p*4 + 1]) / 255.0f;
        dst_pix[p*4 + 2] = static_cast<float>(src_pix[p*4 + 2]) / 255.0f;
        dst_pix[p*4 + 3] = static_cast<float>(src_pix[p*4 + 3]) / 255.0f;
    }
}

int32_t findMemoryTypeIndex(uint32_t memory_type_bits, VkMemoryPropertyFlagBits required = (VkMemoryPropertyFlagBits)0, VkMemoryPropertyFlagBits wanted = (VkMemoryPropertyFlagBits)0)
{
	const VkPhysicalDeviceMemoryProperties& props = VulkanEngine::get().getPhyDevMemProps();
	int32_t res = -1;
	
	for(size_t i = 0; i < props.memoryTypeCount; i++)
	{
		if((memory_type_bits & (1 << i)) && (props.memoryTypes[i].propertyFlags & required) == required)
		{
			if(wanted != 0)
			{
				int match = props.memoryTypes[i].propertyFlags & wanted;
				if(match == wanted)
					return i;
				else if(match != 0)
					res = i;
			}
			else
				return i;
		}
	}
	
	return res;
}

MyScene::MyScene()
{
	initialize();
}

MyScene::~MyScene()
{
	destroy();
}

void MyScene::initialize()
{
	vkGetDeviceQueue(VulkanEngine::get().getDevice(), VulkanEngine::get().getQueueFamilyIndexGeneral(), 0, &m_queue);
	
	initSynchronizationObjects();
	initCommandBuffers();
	initImage();
	initVertexBuffer();
	initSampler();
	initRecordImages();
	initDescriptorSets();
	
	initSurfaceDependentObjects();
}

void MyScene::initSurfaceDependentObjects()
{
	float ratio = (float)VulkanEngine::get().getSurfaceExtent().width / (float)VulkanEngine::get().getSurfaceExtent().height;
	m_camera = std::make_unique<Camera>(ratio, 1.0f, 1000.0f, M_PI_2);
	
	initRenderTargets();
	initGraphicsPipeline();
	recordRecordCmdBuf();
}

void MyScene::destroySurfaceDependentObjects()
{
	VkDevice d = VulkanEngine::get().getDevice();
	
	if(m_pipeline_layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(d, m_pipeline_layout, VK_NULL_HANDLE);
		m_pipeline_layout = VK_NULL_HANDLE;
	}
	
	if (m_graphics_pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(d, m_graphics_pipeline, VK_NULL_HANDLE);
		m_graphics_pipeline = VK_NULL_HANDLE;
	}
	
	for(auto& rt : m_render_targets)
	{
		rt.destroy();
	}
	
	if (m_render_pass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(d, m_render_pass, VK_NULL_HANDLE);
		m_render_pass = VK_NULL_HANDLE;
	}
}

void MyScene::initSynchronizationObjects()
{
	//fences---------------
	VkFenceCreateInfo fence_create_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT};
	m_fences.resize(VulkanEngine::get().getSwapchainImages().size());
	for(auto& f : m_fences)
	{
		vkCreateFence(VulkanEngine::get().getDevice(), &fence_create_info, VK_NULL_HANDLE, &f);
	}
	
	//semaphores-----------
	VkSemaphoreCreateInfo sem_create_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0};
	m_semaphores.resize(num_sems);
	for(auto& s : m_semaphores)
	{
		vkCreateSemaphore(VulkanEngine::get().getDevice(), &sem_create_info, VK_NULL_HANDLE, &s);
	}
}

void MyScene::destroySynchronizationObjects()
{
	VkDevice d = VulkanEngine::get().getDevice();

	for(auto& f : m_fences)
	{
		vkDestroyFence(d, f, VK_NULL_HANDLE);
		f = VK_NULL_HANDLE;
	}
	
	for(auto& s : m_semaphores)
	{
		if (s != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(d, s, VK_NULL_HANDLE);
			s = VK_NULL_HANDLE;
		}
	}
}

void MyScene::initCommandBuffers()
{
	VulkanEngine& e = VulkanEngine::get();
	
	/*---Initialize primary command pool and primary command buffers---*/
	
	VkCommandPoolCreateInfo command_pool_create_info{};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.pNext = NULL;
	command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	command_pool_create_info.queueFamilyIndex = e.getQueueFamilyIndexGeneral();
	
	vkCreateCommandPool(e.getDevice(), &command_pool_create_info, VK_NULL_HANDLE, &m_primary_command_pool);
	
	/*create a command buffer for each swapchain image*/
	m_command_buffers.resize(e.getSwapchainImages().size());
	
	VkCommandBufferAllocateInfo command_buffer_allocate_info{};
	command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_allocate_info.pNext = NULL;
	command_buffer_allocate_info.commandPool = m_primary_command_pool;
	command_buffer_allocate_info.commandBufferCount = m_command_buffers.size();
	command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	
	vkAllocateCommandBuffers(e.getDevice(), &command_buffer_allocate_info, m_command_buffers.data());
	
	/*---Initialize record command pool and record command buffers---*/
	
	m_record_command_buffers.resize(m_command_buffers.size());
	
	command_pool_create_info.flags = 0;
	
	vkCreateCommandPool(e.getDevice(), &command_pool_create_info, VK_NULL_HANDLE, &m_record_command_pool);
	
	command_buffer_allocate_info.commandPool = m_record_command_pool;
	command_buffer_allocate_info.commandBufferCount = m_record_command_buffers.size();
	command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	
	vkAllocateCommandBuffers(e.getDevice(), &command_buffer_allocate_info, m_record_command_buffers.data());
}

void MyScene::recordRecordCmdBuf()
{
	VulkanEngine& e = VulkanEngine::get();
	
	vkResetCommandPool(e.getDevice(), m_record_command_pool, 0);
	
	/*---Record commands into record command buffers---*/
	
	/*These commands will serve to access contents of rendered image
	on the host, when recording or taking snapshot.*/
	
	VkCommandBufferInheritanceInfo inh_info{};
	inh_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inh_info.pNext = NULL;
	inh_info.renderPass = m_render_pass;
	inh_info.occlusionQueryEnable = VK_FALSE;
	
	VkCommandBufferBeginInfo rec_cmd_buf_begin_info{};
	rec_cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	rec_cmd_buf_begin_info.pNext = NULL;
	rec_cmd_buf_begin_info.flags = 0;
	rec_cmd_buf_begin_info.pInheritanceInfo = &inh_info;
	
	for(size_t i = 0; i < m_record_command_buffers.size(); i++)
	{
		VkCommandBuffer cmd_buf = m_record_command_buffers[i];
		
		vkBeginCommandBuffer(cmd_buf, &rec_cmd_buf_begin_info);
	
			VkImageMemoryBarrier img_to_trans_d{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_record_images[i].img,
			{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
			
			vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &img_to_trans_d);
			
			VkImageBlit blit_reg{};
			blit_reg.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
			blit_reg.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
			blit_reg.srcOffsets[0] = {0, 0, 0};
			blit_reg.srcOffsets[1] = {(int32_t)e.getSurfaceExtent().width, (int32_t)e.getSurfaceExtent().height, 1};
			blit_reg.dstOffsets[0] = {0, 0, 0};
			blit_reg.dstOffsets[1] = {video_res_x, video_res_y, 1};
			
			vkCmdBlitImage(cmd_buf, m_render_targets[i].target_image.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_record_images[i].img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_reg, VK_FILTER_NEAREST);
			
			VkImageMemoryBarrier img_to_trans_s{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_record_images[i].img,
			{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
			
			VkImageMemoryBarrier img1_to_trans_d{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_record_images[i].img1,
			{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
			
			VkImageMemoryBarrier bar1[] = {img_to_trans_s, img1_to_trans_d};
			
			vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, bar1);
			
			VkImageCopy cpy{};
			cpy.srcOffset = {0,0,0};
			cpy.dstOffset = {0,0,0};
			cpy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
			cpy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
			cpy.extent = {video_res_x, video_res_y, 1};
			
			vkCmdCopyImage(cmd_buf, m_record_images[i].img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_record_images[i].img1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);
			
			VkImageMemoryBarrier img1_to_general{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_record_images[i].img1,
			{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
			
			vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &img1_to_general);
	
		vkEndCommandBuffer(cmd_buf);
	}
}

void MyScene::initRecordImages()
{
	VulkanEngine& e = VulkanEngine::get();
	
	m_record_images.resize(e.getSwapchainImages().size());
	
	/*Image to frist blit to, changing format and size*/
	VkImageCreateInfo ri_create_info{};
	ri_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ri_create_info.pNext = NULL;
	ri_create_info.flags = 0;
	ri_create_info.imageType = VK_IMAGE_TYPE_2D;
	ri_create_info.format = VK_FORMAT_R8G8B8A8_UINT;
	ri_create_info.extent = VkExtent3D{video_res_x, video_res_y, 1};
	ri_create_info.mipLevels = 1;
	ri_create_info.arrayLayers = 1;
	ri_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	ri_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	ri_create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ri_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ri_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	/*Staging image to then copy to and read from on the host*/
	VkImageCreateInfo ti_create_info{};
	ti_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ti_create_info.pNext = NULL;
	ti_create_info.flags = 0;
	ti_create_info.imageType = VK_IMAGE_TYPE_2D;
	ti_create_info.format = VK_FORMAT_R8G8B8A8_UINT;
	ti_create_info.extent = VkExtent3D{video_res_x, video_res_y, 1};
	ti_create_info.mipLevels = 1;
	ti_create_info.arrayLayers = 1;
	ti_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	ti_create_info.tiling = VK_IMAGE_TILING_LINEAR;
	ti_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ti_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ti_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	
	VkMemoryRequirements req{};
	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = NULL;
	
	for(auto& ri : m_record_images)
	{
		/*img*/
		vkCreateImage(e.getDevice(), &ri_create_info, VK_NULL_HANDLE, &ri.img);
		
		vkGetImageMemoryRequirements(e.getDevice(), ri.img, &req);
		
		alloc_info.allocationSize = req.size;
		alloc_info.memoryTypeIndex = findMemoryTypeIndex(req.memoryTypeBits);
		
		vkAllocateMemory(e.getDevice(), &alloc_info, VK_NULL_HANDLE, &ri.img_mem);
		
		vkBindImageMemory(e.getDevice(), ri.img, ri.img_mem, 0);
		
		/*img1*/
		vkCreateImage(e.getDevice(), &ti_create_info, VK_NULL_HANDLE, &ri.img1);
		
		vkGetImageMemoryRequirements(e.getDevice(), ri.img1, &req);
		alloc_info.allocationSize = req.size;
		alloc_info.memoryTypeIndex = findMemoryTypeIndex(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		
		vkAllocateMemory(e.getDevice(), &alloc_info, VK_NULL_HANDLE, &ri.img_mem1);
		
		vkBindImageMemory(e.getDevice(), ri.img1, ri.img_mem1, 0);
	}
	
	
}

void MyScene::initSampler()
{
	VkDevice d = VulkanEngine::get().getDevice();
	
	VkSamplerCreateInfo sampler_create_info{};
	sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_create_info.pNext = NULL;
	sampler_create_info.flags = 0;
	sampler_create_info.magFilter = VK_FILTER_LINEAR;
	sampler_create_info.minFilter = VK_FILTER_LINEAR;
	sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_create_info.anisotropyEnable = VK_TRUE;
	sampler_create_info.maxAnisotropy = 4.0f;
	sampler_create_info.unnormalizedCoordinates = VK_FALSE;
	
	vkCreateSampler(d, &sampler_create_info, VK_NULL_HANDLE, &m_sampler);
}

void MyScene::initImage()
{	
	VkDevice d = VulkanEngine::get().getDevice();
	
	VkImageCreateInfo image_create_info{};
	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.pNext = NULL;
	image_create_info.flags = 0;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	image_create_info.extent = VkExtent3D{constants.res_x, constants.res_y, 1};
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
	image_create_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	
	vkCreateImage(d, &image_create_info, VK_NULL_HANDLE, &m_image);
	
	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(d, m_image, &mem_req);
	
	VkMemoryAllocateInfo mem_alloc_info{};
	mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc_info.pNext = NULL;
	mem_alloc_info.allocationSize = mem_req.size;
	mem_alloc_info.memoryTypeIndex = findMemoryTypeIndex(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	
	vkAllocateMemory(d, &mem_alloc_info, VK_NULL_HANDLE, &m_image_memory);
	vkBindImageMemory(d, m_image, m_image_memory, 0);
	
	/*Load image*/
	
	void* ptr;
	vkMapMemory(d, m_image_memory, 0, VK_WHOLE_SIZE, 0, &ptr);
	
	loadImage(img_filename, constants.res_x, constants.res_y, &ptr);
	
	vkUnmapMemory(d, m_image_memory);
	
	/*Create image view*/
	
	VkImageViewCreateInfo iv_create_info{};
	iv_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	iv_create_info.pNext = NULL;
	iv_create_info.flags = 0;
	iv_create_info.image = m_image;
	iv_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	iv_create_info.format = image_create_info.format;
	iv_create_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
	iv_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
	
	vkCreateImageView(d, &iv_create_info, VK_NULL_HANDLE, &m_image_view);
}

void MyScene::destroyImage()
{
	VkDevice d = VulkanEngine::get().getDevice();
	
	if(m_image_view != VK_NULL_HANDLE)
	{
		vkDestroyImageView(d, m_image_view, VK_NULL_HANDLE);
		m_image_view = VK_NULL_HANDLE;
	}
	
	if(m_image != VK_NULL_HANDLE)
	{
		vkDestroyImage(d, m_image, VK_NULL_HANDLE);
		m_image = VK_NULL_HANDLE;
	}
	
	if(m_image_memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(d, m_image_memory, VK_NULL_HANDLE);
		m_image_memory = VK_NULL_HANDLE;
	}
}

void MyScene::initRenderTargets()
{
	VkDevice d = VulkanEngine::get().getDevice();
	
	/*Creating a render target structure for each swapchain image*/
	m_render_targets.resize(VulkanEngine::get().getSwapchainImageViews().size());
	
	/*--Creating target images---*/
	
	/*Target image create info*/
	VkImageCreateInfo image_create_info{};
	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.pNext = NULL;
	image_create_info.flags = 0;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.format = VK_FORMAT_R8G8B8A8_UINT;//VulkanEngine::get().getSurfaceFormat();
	image_create_info.extent = VkExtent3D{VulkanEngine::get().getSurfaceExtent().width, VulkanEngine::get().getSurfaceExtent().height, 1};
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	/*Target image view create info*/
	VkImageViewCreateInfo iv_create_info{};
	iv_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	iv_create_info.pNext = NULL;
	iv_create_info.flags = 0;
	iv_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	iv_create_info.format = image_create_info.format;
	iv_create_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
	iv_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
	
	VkMemoryAllocateInfo mem_alloc_info{};
	mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc_info.pNext = NULL;
	
	VkMemoryRequirements mem_req;
	
	/*Create image for each render target*/
	for(auto& rt : m_render_targets)
	{
		vkCreateImage(d, &image_create_info, VK_NULL_HANDLE, &rt.target_image.img);
		
		vkGetImageMemoryRequirements(d, rt.target_image.img, &mem_req);
		
		mem_alloc_info.allocationSize = mem_req.size;
		mem_alloc_info.memoryTypeIndex = findMemoryTypeIndex(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		vkAllocateMemory(d, &mem_alloc_info, VK_NULL_HANDLE, &rt.target_image.mem);
		vkBindImageMemory(d, rt.target_image.img, rt.target_image.mem, 0);
		
		iv_create_info.image = rt.target_image.img;
		vkCreateImageView(d, &iv_create_info, VK_NULL_HANDLE, &rt.target_image.img_view);
	}
	
	/*---Creating depth stencil buffers---*/
	
	/*Depth stencil image create info*/
	VkImageCreateInfo ds_img_create_info{};
	ds_img_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ds_img_create_info.pNext = NULL;
	ds_img_create_info.flags = 0;
	ds_img_create_info.imageType = VK_IMAGE_TYPE_2D;
	ds_img_create_info.format = VK_FORMAT_D32_SFLOAT;
	ds_img_create_info.extent = VkExtent3D{VulkanEngine::get().getSurfaceExtent().width,VulkanEngine::get().getSurfaceExtent().height, 1};
	ds_img_create_info.mipLevels = 1;
	ds_img_create_info.arrayLayers = 1;
	ds_img_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	ds_img_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	ds_img_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	ds_img_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ds_img_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	/*Depth stencil image view create info*/
	VkImageViewCreateInfo ds_img_view_create_info{};
	ds_img_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ds_img_view_create_info.pNext = NULL;
	ds_img_view_create_info.flags = 0;
	ds_img_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ds_img_view_create_info.format = ds_img_create_info.format;
	ds_img_view_create_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
	ds_img_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
	
	/*Create a depth stencil buffer for each render target*/
	for(auto& rt : m_render_targets)
	{
		/*Create image*/
		vkCreateImage(d, &ds_img_create_info, VK_NULL_HANDLE, &rt.depth_stencil_buffer.img);
		
		/*Get given image memory requirements*/
		vkGetImageMemoryRequirements(d, rt.depth_stencil_buffer.img, &mem_req);
		
		/*Update memory allocation info based on acquired requirements*/
		mem_alloc_info.allocationSize = mem_req.size;
		mem_alloc_info.memoryTypeIndex = findMemoryTypeIndex(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		/*Allocate and bind memory to given image*/
		vkAllocateMemory(d, &mem_alloc_info, VK_NULL_HANDLE, &rt.depth_stencil_buffer.mem);
		vkBindImageMemory(d, rt.depth_stencil_buffer.img, rt.depth_stencil_buffer.mem, 0);
		
		/*Create image view for given image*/
		ds_img_view_create_info.image = rt.depth_stencil_buffer.img;
		vkCreateImageView(d, &ds_img_view_create_info, VK_NULL_HANDLE, &rt.depth_stencil_buffer.img_view);
	}
	
	
	/*---Creating render pass---*/
	
	VkAttachmentDescription at_desc[3]{};
	
	/*color attachment - target image*/
	at_desc[0].flags = 0;
	at_desc[0].format = image_create_info.format; //use image create info defined earlier in this function
	at_desc[0].samples = VK_SAMPLE_COUNT_1_BIT;
	at_desc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	at_desc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	at_desc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	at_desc[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	
	/*color attachment - swapchain image*/
	at_desc[1].flags = 0;
	at_desc[1].format = VulkanEngine::get().getSurfaceFormat();
	at_desc[1].samples = VK_SAMPLE_COUNT_1_BIT;
	at_desc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	at_desc[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	at_desc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	at_desc[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	
	/*depth stencil attachment - depth stencil buffer*/
	at_desc[2].flags = 0;
	at_desc[2].format = VK_FORMAT_D32_SFLOAT;
	at_desc[2].samples = VK_SAMPLE_COUNT_1_BIT;
	at_desc[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	at_desc[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	at_desc[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	at_desc[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	
	std::vector<VkAttachmentReference> col_at_ref =
	{
		{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
	};
	
	VkAttachmentReference ds_at_ref{2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
	
	VkSubpassDescription sub_desc{};
	sub_desc.flags = 0;
	sub_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub_desc.inputAttachmentCount = 0;
	sub_desc.pInputAttachments = NULL;
	sub_desc.colorAttachmentCount = col_at_ref.size();
	sub_desc.pColorAttachments = col_at_ref.data();
	sub_desc.pResolveAttachments = NULL;
	sub_desc.pDepthStencilAttachment = &ds_at_ref;
	sub_desc.preserveAttachmentCount = 0;
	sub_desc.pPreserveAttachments = NULL;
	
	VkRenderPassCreateInfo render_pass_create_info{};
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.pNext = NULL;
	render_pass_create_info.flags = 0;
	render_pass_create_info.attachmentCount = 3;
	render_pass_create_info.pAttachments = at_desc;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &sub_desc;
	render_pass_create_info.dependencyCount = 0;
	render_pass_create_info.pDependencies = NULL;
	
	vkCreateRenderPass(VulkanEngine::get().getDevice(), &render_pass_create_info, VK_NULL_HANDLE, &m_render_pass);
	
	/*---Creating framebuffers---*/
	/*Create a set of identical framebuffers, one for each swapchain image*/
	
	VkFramebufferCreateInfo frambuffer_create_info{};
	frambuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frambuffer_create_info.pNext = NULL;
	frambuffer_create_info.flags = 0;
	frambuffer_create_info.renderPass = m_render_pass;
	frambuffer_create_info.attachmentCount = 3;
	frambuffer_create_info.width = VulkanEngine::get().getSurfaceExtent().width;
	frambuffer_create_info.height = VulkanEngine::get().getSurfaceExtent().height;
	frambuffer_create_info.layers = 1;
	
	/*Specify clear values used for each framebuffer*/
	std::vector<VkClearValue> cv = {
            {VkClearColorValue{0, 0, 0, 1.0f}},
            {VkClearColorValue{0, 0, 0, 1.0f}},
			VkClearValue{.depthStencil=VkClearDepthStencilValue{1.0f}}
		};
	
	/*Specify render pass begin info fields, which are identical for each framebuffer,
	specify the variant ones later, per framebuffer*/
	
	VkRenderPassBeginInfo render_pass_begin_info{};
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = m_render_pass;
	render_pass_begin_info.renderArea.extent = VulkanEngine::get().getSurfaceExtent();
	render_pass_begin_info.renderArea.offset = VkOffset2D{0,0};
	render_pass_begin_info.clearValueCount = cv.size();
	
	/*For each render target...*/
	for(size_t i = 0; i < m_render_targets.size(); i++)
	{
		/*Specify the attachments for each framebuffer as a different swapchain image view and different depth stencil buffer*/
		VkImageView attachments[] = {m_render_targets[i].target_image.img_view, VulkanEngine::get().getSwapchainImageViews()[i], m_render_targets[i].depth_stencil_buffer.img_view};
		frambuffer_create_info.pAttachments = attachments;
		/*Create a framebuffer for given render target*/
		vkCreateFramebuffer(VulkanEngine::get().getDevice(), &frambuffer_create_info, VK_NULL_HANDLE, &m_render_targets[i].framebuffer);
		
		/*Set the same clear values (defined earlier) for each render target*/
		m_render_targets[i].clear_values = cv;
		
		/*Set the render pass for given render target*/
		m_render_targets[i].render_pass = m_render_pass;
		
		/*Set the render pass begin info for each render target
		and set the variant fields specifically*/
		m_render_targets[i].begin_info = render_pass_begin_info;
		m_render_targets[i].begin_info.framebuffer = m_render_targets[i].framebuffer;
		m_render_targets[i].begin_info.pClearValues = m_render_targets[i].clear_values.data();
	}
}

void MyScene::initVertexBuffer()
{
	VulkanEngine& e = VulkanEngine::get();
	
	/*---Creating buffer---*/
	
	/*Specify usage as storage texel buffer to store formatted vertex data,
	that can be read and written inside shaders*/
	VkBufferCreateInfo vb_create_info{};
	vb_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vb_create_info.pNext = NULL;
	vb_create_info.flags = 0;
	vb_create_info.size = sizeof(Vertex) * constants.res_x * constants.res_y;
	vb_create_info.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
	vb_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vb_create_info.queueFamilyIndexCount = 1;
	uint32_t qfi = e.getQueueFamilyIndexGeneral();
	vb_create_info.pQueueFamilyIndices = &qfi;
	
	vkCreateBuffer(e.getDevice(), &vb_create_info, VK_NULL_HANDLE, &m_vertex_buffer);
	
	/*Get memory requirements for buffer*/
	VkMemoryRequirements vb_mem_req;
	vkGetBufferMemoryRequirements(e.getDevice(), m_vertex_buffer, &vb_mem_req);
	
	VkMemoryAllocateInfo vb_mem_alloc_info{};
	vb_mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vb_mem_alloc_info.pNext = NULL;
	vb_mem_alloc_info.allocationSize = vb_mem_req.size;
	vb_mem_alloc_info.memoryTypeIndex = findMemoryTypeIndex(vb_mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	
	/*Allocate memory for the buffer*/
	vkAllocateMemory(e.getDevice(), &vb_mem_alloc_info, VK_NULL_HANDLE, &m_vertex_buffer_memory);
	
	/*Map buffer memory*/
	Vertex* mem;
	vkMapMemory(e.getDevice(), m_vertex_buffer_memory, 0, VK_WHOLE_SIZE, 0, (void**)&mem);
	
	const uint32_t numVerts = constants.res_x * constants.res_y;
	
	/*Generate random initial location for each vertex, within specified bounds*/
	for(size_t v = 0; v < numVerts; v++)
	{
		mem[v].pos.x = (2*float((float)std::rand() / (float)RAND_MAX) - 1.0f) * x_bound;
		mem[v].pos.y = (2*float((float)std::rand() / (float)RAND_MAX) - 1.0f) * y_bound;
		mem[v].pos.z = (2*float((float)std::rand() / (float)RAND_MAX) - 1.0f) * z_bound;
	}
	
	/*Unmap buffer memory*/
	vkUnmapMemory(e.getDevice(), m_vertex_buffer_memory);
	
	/*Bind memory to the buffer*/
	vkBindBufferMemory(e.getDevice(), m_vertex_buffer, m_vertex_buffer_memory, 0);
	
	/*---Create vertex buffer view---*/
	
	VkBufferViewCreateInfo vb_view_create_info{};
	vb_view_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	vb_view_create_info.pNext = NULL;
	vb_view_create_info.flags = 0;
	vb_view_create_info.buffer = m_vertex_buffer;
	vb_view_create_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vb_view_create_info.offset = 0;
	vb_view_create_info.range = VK_WHOLE_SIZE;
	
	vkCreateBufferView(e.getDevice(), &vb_view_create_info, VK_NULL_HANDLE, &m_vertex_buffer_view);
}

void MyScene::initDescriptorSets()
{
	VkDevice d = VulkanEngine::get().getDevice();
	
	/*---Create descriptor set layout---*/
	
	VkDescriptorSetLayoutBinding vb_binding{};
	vb_binding.binding = 0;
	vb_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	vb_binding.descriptorCount = 1;
	vb_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vb_binding.pImmutableSamplers = NULL;
	
	VkDescriptorSetLayoutBinding img_binding{};
	img_binding.binding = 1;
	img_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	img_binding.descriptorCount = 1;
	img_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	img_binding.pImmutableSamplers = &m_sampler;
	
	std::vector<VkDescriptorSetLayoutBinding> bindings{vb_binding, img_binding};
	
	VkDescriptorSetLayoutCreateInfo desc_set_layout_create_info{};
	desc_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc_set_layout_create_info.pNext = NULL;
	desc_set_layout_create_info.flags = 0;
	desc_set_layout_create_info.bindingCount = bindings.size();
	desc_set_layout_create_info.pBindings = bindings.data();
	
	vkCreateDescriptorSetLayout(d, &desc_set_layout_create_info, VK_NULL_HANDLE, &m_descriptor_set_layout);
	
	/*---Create descriptor pool---*/
	
	std::vector<VkDescriptorPoolSize> pool_sizes{
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
	};
	
	VkDescriptorPoolCreateInfo desc_pool_create_info{};
	desc_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	desc_pool_create_info.pNext = NULL;
	desc_pool_create_info.flags = 0;
	desc_pool_create_info.maxSets = 1;
	desc_pool_create_info.poolSizeCount = pool_sizes.size();
	desc_pool_create_info.pPoolSizes = pool_sizes.data();
	
	vkCreateDescriptorPool(d, &desc_pool_create_info, VK_NULL_HANDLE, &m_descriptor_pool);
	
	VkDescriptorSetAllocateInfo desc_set_allocate_info{};
	desc_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	desc_set_allocate_info.pNext = NULL;
	desc_set_allocate_info.descriptorPool = m_descriptor_pool;
	desc_set_allocate_info.descriptorSetCount = 1;
	desc_set_allocate_info.pSetLayouts = &m_descriptor_set_layout;
	
	vkAllocateDescriptorSets(d, &desc_set_allocate_info, &m_descriptor_set);
	
	VkWriteDescriptorSet vb_write{};
	vb_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	vb_write.pNext = NULL;
	vb_write.dstSet = m_descriptor_set;
	vb_write.dstBinding = 0;
	vb_write.dstArrayElement = 0;
	vb_write.descriptorCount = 1;
	vb_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	vb_write.pTexelBufferView = &m_vertex_buffer_view;
	
	VkDescriptorImageInfo img_info{};
	img_info.sampler = m_sampler;
	img_info.imageView = m_image_view;
	img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	VkWriteDescriptorSet img_write{};
	img_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	img_write.pNext = NULL;
	img_write.dstSet = m_descriptor_set;
	img_write.dstBinding = 1;
	img_write.dstArrayElement = 0;
	img_write.descriptorCount = 1;
	img_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	img_write.pImageInfo = &img_info;
	
	std::vector<VkWriteDescriptorSet> writes{vb_write, img_write};
	
	vkUpdateDescriptorSets(d, writes.size(), writes.data(), 0, NULL);
}

void MyScene::initGraphicsPipeline()
{
	auto vs = std::make_unique<Shader>("vs.spv");
	auto fs = std::make_unique<Shader>("fs.spv");
	
	VkPipelineShaderStageCreateInfo vs_stage{};
	vs_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vs_stage.pNext = NULL;
	vs_stage.flags = 0;
	vs_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vs_stage.module = vs->getModule();
	vs_stage.pName = "main";
	vs_stage.pSpecializationInfo = NULL;
	
	VkPipelineShaderStageCreateInfo fs_stage{};
	fs_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fs_stage.pNext = NULL;
	fs_stage.flags = 0;
	fs_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fs_stage.module = fs->getModule();
	fs_stage.pName = "main";
	fs_stage.pSpecializationInfo = NULL;
	
	std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {vs_stage, fs_stage};
	
	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
	vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state_create_info.pNext = NULL;
	vertex_input_state_create_info.flags = 0;
	vertex_input_state_create_info.vertexBindingDescriptionCount = 0;//v_in_bind_desc.size();
	vertex_input_state_create_info.pVertexBindingDescriptions = NULL;//v_in_bind_desc.data();
	vertex_input_state_create_info.vertexAttributeDescriptionCount = 0;//v_in_att_desc.size();
	vertex_input_state_create_info.pVertexAttributeDescriptions = NULL;//v_in_att_desc.data();
	
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = NULL;
	input_assembly_state.flags = 0;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;
	
	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = VulkanEngine::get().getSurfaceExtent().width;
	viewport.height = VulkanEngine::get().getSurfaceExtent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	
	VkRect2D scissors{VkOffset2D{0,0}, VulkanEngine::get().getSurfaceExtent()};
	
	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissors;
	
	VkPipelineRasterizationStateCreateInfo rast_state{};
	rast_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rast_state.pNext = NULL;
	rast_state.flags = 0;
	rast_state.depthClampEnable = VK_FALSE;
	rast_state.rasterizerDiscardEnable = VK_FALSE;
	rast_state.polygonMode = VK_POLYGON_MODE_FILL;
	rast_state.cullMode = VK_CULL_MODE_NONE;
	rast_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rast_state.depthBiasEnable = VK_FALSE;
	rast_state.lineWidth = 1.0f;
	
	VkPipelineMultisampleStateCreateInfo multi_state{};
	multi_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multi_state.pNext = NULL;
	multi_state.flags = 0;
	multi_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multi_state.sampleShadingEnable = VK_TRUE;
	multi_state.minSampleShading = 1.0f;
	multi_state.pSampleMask = NULL;
	multi_state.alphaToCoverageEnable = VK_FALSE;
	multi_state.alphaToOneEnable = VK_FALSE;
	
	VkPipelineColorBlendAttachmentState att_state[2]{};
	
	att_state[0].blendEnable = VK_FALSE;
	att_state[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	
	att_state[1].blendEnable = VK_FALSE;
	att_state[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	
	VkPipelineColorBlendStateCreateInfo blend_state{};
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.attachmentCount = 2;
	blend_state.pAttachments = att_state;
	
	VkPipelineDepthStencilStateCreateInfo depth_stensil_state{};
	depth_stensil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stensil_state.pNext = NULL;
	depth_stensil_state.flags = 0;
	depth_stensil_state.depthTestEnable = VK_TRUE;
	depth_stensil_state.depthWriteEnable = VK_TRUE;
	depth_stensil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stensil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stensil_state.stencilTestEnable = VK_FALSE;
	
	VkPushConstantRange constant_ranges{};
	
	constant_ranges.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	constant_ranges.offset = 0;
	constant_ranges.size = sizeof(s_constants);	
	
	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.pNext = NULL;
	layout_info.flags = 0;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &m_descriptor_set_layout;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &constant_ranges;
	
	vkCreatePipelineLayout(VulkanEngine::get().getDevice(), &layout_info, VK_NULL_HANDLE, &m_pipeline_layout);
	
	VkGraphicsPipelineCreateInfo g_pipeline_create_info{};
	g_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	g_pipeline_create_info.pNext = NULL;
	g_pipeline_create_info.flags = 0;
	g_pipeline_create_info.stageCount = shader_stages.size();
	g_pipeline_create_info.pStages = shader_stages.data();
	g_pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
	g_pipeline_create_info.pInputAssemblyState = &input_assembly_state;
	g_pipeline_create_info.pTessellationState = NULL;
	g_pipeline_create_info.pViewportState = &viewport_state;
	g_pipeline_create_info.pRasterizationState = &rast_state;
	g_pipeline_create_info.pMultisampleState = &multi_state;
	g_pipeline_create_info.pDepthStencilState = &depth_stensil_state;
	g_pipeline_create_info.pColorBlendState = &blend_state;
	g_pipeline_create_info.pDynamicState = NULL;
	g_pipeline_create_info.layout = m_pipeline_layout;
	g_pipeline_create_info.renderPass = m_render_pass;
	g_pipeline_create_info.subpass = 0;
	g_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	g_pipeline_create_info.basePipelineIndex = -1;
	
	vkCreateGraphicsPipelines(VulkanEngine::get().getDevice(), VK_NULL_HANDLE, 1, &g_pipeline_create_info, VK_NULL_HANDLE, &m_graphics_pipeline);
}

void MyScene::destroy()
{
	VkDevice d = VulkanEngine::get().getDevice();
	
	destroySynchronizationObjects();
	
	destroySurfaceDependentObjects();
	
	for(auto& ri : m_record_images)
	{
		ri.destroy();
	}
	
	if(m_sampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(d, m_sampler, VK_NULL_HANDLE);
		m_sampler = VK_NULL_HANDLE;
	}
	
	destroyImage();
	
	if (m_primary_command_pool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(d, m_primary_command_pool, VK_NULL_HANDLE);
		m_primary_command_pool = VK_NULL_HANDLE;
	}
	
	if (m_record_command_pool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(d, m_record_command_pool, VK_NULL_HANDLE);
		m_record_command_pool = VK_NULL_HANDLE;
	}
	
	if(m_vertex_buffer_view != VK_NULL_HANDLE)
	{
		vkDestroyBufferView(d, m_vertex_buffer_view, VK_NULL_HANDLE);
		m_vertex_buffer_view = VK_NULL_HANDLE;
	}
	
	if(m_vertex_buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(d, m_vertex_buffer, VK_NULL_HANDLE);
		m_vertex_buffer = VK_NULL_HANDLE;
	}
	
	if(m_vertex_buffer_memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(d, m_vertex_buffer_memory, VK_NULL_HANDLE);
		m_vertex_buffer_memory = VK_NULL_HANDLE;
	}
	
	if(m_descriptor_set != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(d, m_descriptor_pool, VK_NULL_HANDLE);
		m_descriptor_pool = VK_NULL_HANDLE;
	}
	
	if(m_descriptor_set_layout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(d, m_descriptor_set_layout, VK_NULL_HANDLE);
		m_descriptor_set_layout = VK_NULL_HANDLE;
	}
}

Timer& MyScene::getTimer()
{
	return m_timer;
}

void MyScene::onResize()
{
	destroySurfaceDependentObjects();
	initSurfaceDependentObjects();
}

void MyScene::recordFrame(uint32_t id)
{
	VkDevice d = VulkanEngine::get().getDevice();
	
	vkWaitForFences(d, 1, &m_fences[id], VK_TRUE, UINT64_MAX);
	
	uint8_t* data;
	vkMapMemory(d, m_record_images[id].img_mem1, 0, VK_WHOLE_SIZE, 0, (void**)&data);
	
	m_recorder.nextFrame(data);
	
	vkUnmapMemory(d, m_record_images[id].img_mem1);
}

void takeSnapshot(VkFence fence, VkDeviceMemory mem)
{
	VkDevice d = VulkanEngine::get().getDevice();
	
	vkWaitForFences(d, 1, &fence, VK_TRUE, UINT64_MAX);
	
	uint8_t* data;
	vkMapMemory(d, mem, 0, VK_WHOLE_SIZE, 0, (void**)&data);
	
    QImage snap(data, video_res_x, video_res_y, QImage::Format_RGBA8888);
    /*Quality set to -1, which chooses default; can be explicitly specified from 0 to 100*/
    snap.save("test.png", "PNG", -1);
	
	vkUnmapMemory(d, mem);
}

void MyScene::update()
{
    auto& w = VulkanEngine::get().getWindow();

	float dt = m_timer.getDeltaTime();
    float speed = w->getKeyState(Qt::Key_Shift) ? 100.0f : 40.0f;
	
    if(w->getKeyState(Qt::Key_W)) m_camera->walk(speed*dt);
    if(w->getKeyState(Qt::Key_S)) m_camera->walk(-speed*dt);
    if(w->getKeyState(Qt::Key_A)) m_camera->strafe(-speed*dt);
    if(w->getKeyState(Qt::Key_D)) m_camera->strafe(speed*dt);
    if(w->getKeyState(Qt::Key_Space)) m_camera->upDown(speed*dt);
    if(w->getKeyState(Qt::Key_C)) m_camera->upDown(-speed*dt);
	
	constants.dt = m_timer.getDeltaTime();
	constants.vp = m_camera->getViewProj();
	
	constants.eyeW = m_camera->getCamPosW();
    constants.curDirNW = glm::normalize(glm::affineInverse(m_camera->getView()) * glm::vec4(m_camera->getCurPosProj(*VulkanEngine::get().getWindow()), 0.0f));
}

void MyScene::render()
{
	update();
	
	VulkanEngine& e = VulkanEngine::get();
	uint32_t image_index;
	vkAcquireNextImageKHR(e.getDevice(), e.getSwapchain(), UINT64_MAX, m_semaphores[s_acquire_image], VK_NULL_HANDLE, &image_index);
	
	VkCommandBuffer cmd_buf = m_command_buffers[image_index];
	
	VkCommandBufferBeginInfo command_buffer_begin_info{};
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.pNext = NULL;
	command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	command_buffer_begin_info.pInheritanceInfo = NULL;
	
	vkWaitForFences(e.getDevice(), 1, &m_fences[image_index], VK_TRUE, UINT64_MAX);
	
	vkBeginCommandBuffer(cmd_buf, &command_buffer_begin_info);
	
	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
	
	vkCmdPushConstants(cmd_buf, m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(s_constants), &constants);
	
	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1, &m_descriptor_set, 0, NULL);
	
		vkCmdBeginRenderPass(cmd_buf, &m_render_targets[image_index].begin_info, VK_SUBPASS_CONTENTS_INLINE);
	
		vkCmdDraw(cmd_buf, constants.res_x*constants.res_y, 1, 0, 0);
	
		vkCmdEndRenderPass(cmd_buf);
		
	if(recording || snap)
		vkCmdExecuteCommands(cmd_buf, 1, &m_record_command_buffers[image_index]);
		
	vkEndCommandBuffer(cmd_buf);
	
	VkPipelineStageFlags submit_wait_flags[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &m_semaphores[s_submit];
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &m_semaphores[s_acquire_image];
	submit_info.pWaitDstStageMask = submit_wait_flags;
	
	vkResetFences(e.getDevice(), 1, &m_fences[image_index]);
	vkQueueSubmit(m_queue, 1, &submit_info, m_fences[image_index]);
	
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pImageIndices = &image_index;
	present_info.pNext = NULL;
	present_info.pResults = NULL;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &e.getSwapchain();
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &m_semaphores[s_submit];
	
	vkQueuePresentKHR(m_queue, &present_info);
	
	if(snap)
	{
		takeSnapshot(m_fences[image_index], m_record_images[image_index].img_mem1);
		snap = false;
	}
	
	if(recording)
		auto f = std::async(std::launch::async, &MyScene::recordFrame, this, image_index);
}


void MyScene::keyPressEvent(QKeyEvent* e)
{
    switch(e->key())
    {
    case Qt::Key_Escape:
        VulkanEngine::get().stop();
        break;
    case Qt::Key_Q:
        VulkanEngine::get().getWindow()->showCursor(cam_move);
        cam_move = !cam_move;
        break;
    case Qt::Key_P:
        m_timer.toggle();
        break;
    case Qt::Key_R:
        if(!recording)
        {
            m_recorder.startRecording(vid_filename, video_res_x, video_res_y, video_res_x, video_res_y, video_fps);
            recording = true;
        }
        else
        {
            m_recorder.stopRecording();
            recording = false;
        }
        break;
    case Qt::Key_X:
        snap = true;
        break;
    default:
        break;
    }
}

void MyScene::keyReleaseEvent(QKeyEvent *event)
{

}

void MyScene::mousePressEvent(QMouseEvent* e)
{
    if(e->button() == Qt::RightButton)
        constants.particle_speed = top_speed;
}

void MyScene::mouseReleaseEvent(QMouseEvent* e)
{
    if(e->button() == Qt::RightButton)
        constants.particle_speed = min_speed;
}

void MyScene::mouseDoubleClickEvent(QMouseEvent *event)
{

}

void MyScene::mouseMoveEvent(QMouseEvent* e)
{
    int16_t dx = e->x() - m_lastX;
    int16_t dy = e->y() - m_lastY;

    m_lastX = e->x();
    m_lastY = e->y();

    if(cam_move)
    {
        m_camera->rotate(dx*0.01f);
        m_camera->pitch(dy*0.01f);
    }
}

void MyScene::wheelEvent(QWheelEvent *event)
{

}
