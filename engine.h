#ifndef ENGINE_H
#define ENGINE_H

#define VK_USE_PLATFORM_XCB_KHR

#include <vulkan/vulkan.h>

#ifdef VK_USE_PLATFORM_XCB_KHR
#define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif

#include <memory>
#include <vector>

#include "VulkanWindow.h"
#include "Timer.h"
#include "Scene.h"

class VulkanEngine
{
public:
	static VulkanEngine& get();
	~VulkanEngine();

	void run();
	void stop();

	void onResize();
	
	void setScene(std::shared_ptr<Scene>);

	std::unique_ptr<VulkanWindow>& getWindow();
    Scene& getScene();
	
	const VkDevice& getDevice() const noexcept;
	const VkSwapchainKHR& getSwapchain() const noexcept;
	const std::vector<VkImageView>& getSwapchainImageViews()const noexcept;
	const std::vector<VkImage>& getSwapchainImages() const noexcept;
	const VkPhysicalDeviceMemoryProperties& getPhyDevMemProps() const noexcept;
	
	VkFormat getSurfaceFormat() const noexcept;
	VkExtent2D getSurfaceExtent() const noexcept;
	uint32_t getQueueFamilyIndexGeneral();
	uint32_t getQueueFamilyIndexTransfer();
	
private:
	VulkanEngine();
	void render();

	void EnableLayersAndExtensions();

	bool InitInstance();
	bool InitDevice();

	//SURFACE DEPENDENT-----------------------------------
	bool InitSurfaceDependentObjects();
	void DeinitSurfaceDependentObjects();
	bool InitSurface();
	bool InitSwapchain();

	void InitDebug();
	void DeInitDebug();
	
	bool Initialize();
	void Destroy();

	///////////////////////////////////////////////////////////////////////////////
	///////////////				SURFACE INDEPENDENT					///////////////
	///////////////////////////////////////////////////////////////////////////////

	//LAYERS AND EXTENSIONS--------------------------------------------------------
	std::vector<const char*> m_instance_layers;
	std::vector<const char*> m_instance_extensions;
	std::vector<const char*> m_device_extensions;

	VkDebugReportCallbackEXT debug_report = VK_NULL_HANDLE;
	
	//INSTANCE---------------------------------------------------------------------
	VkInstance m_instance = VK_NULL_HANDLE;

	//PHYSICAL DEVICE--------------------------------------------------------------
	VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

	VkPhysicalDeviceProperties m_physical_device_properties;
	VkPhysicalDeviceFeatures m_physical_device_features;
	VkPhysicalDeviceMemoryProperties m_physical_device_memory_properties;

	//DEVICE-----------------------------------------------------------------------
	VkDevice m_device = VK_NULL_HANDLE;
	uint32_t m_queue_family_index_general = -1;
	uint32_t m_queue_family_index_transfer = -1;

	///////////////////////////////////////////////////////////////////////////////
	///////////////				SURFACE DEPENDENT					///////////////
	///////////////////////////////////////////////////////////////////////////////

	//SURFACE AND SWAPCHAIN--------------------------------------------------------
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;

	VkExtent2D m_surface_extent;
	VkFormat m_surface_format;

	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchain_images;
	std::vector<VkImageView> m_swapchain_image_views;
	
	std::unique_ptr<VulkanWindow> m_window;
	std::shared_ptr<Scene> m_scene;
	
	bool m_running = true;
};

#endif //ENGINE_H
