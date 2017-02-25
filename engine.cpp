#include "engine.h"
#include "debug.h"

#include <QtX11Extras/QX11Info>
#include <QApplication>

#include <iostream>
#include <string>

#include "vulkan_math.h"

using namespace std;

VulkanEngine& VulkanEngine::get()
{
	static VulkanEngine m_ptr;
	
	return m_ptr;
}

VulkanEngine::VulkanEngine()
{
	/*if initialization fails, destroy whatever was created*/
	if(!Initialize())
	{
		Destroy();
	}
}

VulkanEngine::~VulkanEngine()
{
	Destroy();
}

void VulkanEngine::stop()
{
	m_running = false;
}

void VulkanEngine::EnableLayersAndExtensions()
{
	uint32_t count;
	
//extensions-------------------------------------
	
	m_instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    m_instance_extensions.push_back(VK_PLATFORM_SURFACE_EXTENSION_NAME);

	m_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

//layers-----------------------------------------

	std::vector<VkLayerProperties> l_props;
	vkEnumerateInstanceLayerProperties(&count, VK_NULL_HANDLE);
	l_props.resize(count);
	vkEnumerateInstanceLayerProperties(&count, l_props.data());
	
#ifndef NDEBUG
	m_instance_layers.push_back("VK_LAYER_LUNARG_standard_validation");
	m_instance_layers.push_back("VK_LAYER_LUNARG_monitor");
	//m_instance_layers.push_back("VK_LAYER_LUNARG_api_dump");
	
	m_instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif//NDEBUG
}

bool VulkanEngine::Initialize()
{
	bool res;

	m_window = std::make_unique<VulkanWindow>();

	EnableLayersAndExtensions();

	res = InitInstance();
	if (res == false)
		return false;

	InitDebug();

	res = InitDevice();
	if (res == false)
		return false;

	res = InitSurfaceDependentObjects();
	if (res == false)
		return false;

	return true;
}

void VulkanEngine::Destroy()
{
	//ShowCursor(TRUE);
	
	/*wait for the device to become idle before destroying anything*/
	if (m_device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(m_device);
	}

	/*delete scene first*/
	m_scene.reset();

	DeinitSurfaceDependentObjects();

	if (m_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(m_device, VK_NULL_HANDLE);
		m_device = VK_NULL_HANDLE;
	}

	DeInitDebug();

	if (m_instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(m_instance, VK_NULL_HANDLE);
		m_instance = VK_NULL_HANDLE;
	}
}

//surface independent--------------------------------

bool VulkanEngine::InitInstance()
{
	VkResult res;

	VkApplicationInfo application_info{};
	application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application_info.pNext = NULL;
	application_info.pApplicationName = "VulkanApp";
	application_info.apiVersion = VK_MAKE_VERSION(1, 0, 39);
	application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	application_info.pEngineName = nullptr;
	application_info.engineVersion = 0;

	VkInstanceCreateInfo instance_create_info{};
	instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.pNext = NULL;
	instance_create_info.flags = 0;
	instance_create_info.pApplicationInfo = &application_info;
	instance_create_info.enabledLayerCount = m_instance_layers.size();
	instance_create_info.ppEnabledLayerNames = m_instance_layers.data();
	instance_create_info.enabledExtensionCount = m_instance_extensions.size();
	instance_create_info.ppEnabledExtensionNames = m_instance_extensions.data();

	res = vkCreateInstance(&instance_create_info, nullptr, &m_instance);
	if (res < 0)
	{
		ErrorMessage("Error: Failed to create vulkan instance.", res);
		return false;
	}

	return true;
}

bool VulkanEngine::InitDevice()
{
	uint32_t device_count;
	vkEnumeratePhysicalDevices(m_instance, &device_count, VK_NULL_HANDLE);
	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

	m_physical_device = devices[0];

	vkGetPhysicalDeviceProperties(m_physical_device, &m_physical_device_properties);
	vkGetPhysicalDeviceFeatures(m_physical_device, &m_physical_device_features);
	vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_physical_device_memory_properties);

	uint32_t family_count;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &family_count, VK_NULL_HANDLE);
	std::vector<VkQueueFamilyProperties> queue_families(family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &family_count, queue_families.data());

	for (uint32_t i = 0; i < family_count; i++)
	{
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			if (m_queue_family_index_general == -1)
				m_queue_family_index_general = i;
		}
		else if (queue_families[i].queueFlags == VK_QUEUE_TRANSFER_BIT)
		{
			if (m_queue_family_index_transfer == -1)
				m_queue_family_index_transfer = i;
		}
	}

	float queue_priorities[] = { 1.0f };

	VkDeviceQueueCreateInfo queue_create_info{};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueCount = 1;
	queue_create_info.queueFamilyIndex = m_queue_family_index_general;
	queue_create_info.pQueuePriorities = queue_priorities;

	VkDeviceCreateInfo device_create_info{};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.enabledExtensionCount = m_device_extensions.size();
	device_create_info.ppEnabledExtensionNames = m_device_extensions.data();
	device_create_info.enabledLayerCount = 0;
	device_create_info.ppEnabledLayerNames = NULL;
	device_create_info.queueCreateInfoCount = 1;
	device_create_info.pQueueCreateInfos = &queue_create_info;
	device_create_info.pEnabledFeatures = &m_physical_device_features;

	VkResult res = vkCreateDevice(m_physical_device, &device_create_info, VK_NULL_HANDLE, &m_device);
	if (res < 0)
	{
		ErrorMessage("Error: Failed to create logical device.", res);
		return false;
	}

	return true;
}

//surface dependent----------------------------------

void VulkanEngine::onResize()
{
	vkDeviceWaitIdle(m_device);
	
	DeinitSurfaceDependentObjects();

	InitSurfaceDependentObjects();
	
	m_scene->onResize();
}

bool VulkanEngine::InitSurfaceDependentObjects()
{
	bool res;

	res = InitSurface();
	if (res == false)
		return false;

	res = InitSwapchain();
	if (res == false)
		return false;

	return true;
}

void VulkanEngine::DeinitSurfaceDependentObjects()
{
	for (auto& siv : m_swapchain_image_views)
	{
		vkDestroyImageView(m_device, siv, VK_NULL_HANDLE);
	}
	
	m_swapchain_image_views.clear();

	if (m_swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_device, m_swapchain, VK_NULL_HANDLE);
	}

	if (m_surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, VK_NULL_HANDLE);
		m_surface = VK_NULL_HANDLE;
	}

	m_swapchain_images.clear();
}

bool VulkanEngine::InitSwapchain()
{
	VkResult res;

	VkSurfaceCapabilitiesKHR surface_capabilities;
	std::vector<VkSurfaceFormatKHR> surface_formats;
	std::vector<VkPresentModeKHR> present_modes;

	VkSurfaceFormatKHR surface_format;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t image_count;

	res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilities);
	if (res < 0)
	{
		ErrorMessage("Error: Failed to get physical device surface capabilities.", res);
		return false;
	}
	
	if (surface_capabilities.currentExtent.width == UINT32_MAX)
	{
        m_surface_extent.width = m_window->width();
        m_surface_extent.height = m_window->height();
	}
	else
	{
		m_surface_extent = surface_capabilities.currentExtent;
	}
	
	image_count = surface_capabilities.minImageCount + 1;

	uint32_t surface_format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &surface_format_count, VK_NULL_HANDLE);
	surface_formats.resize(surface_format_count);
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &surface_format_count, surface_formats.data());
	if (res < 0)
	{
		ErrorMessage("Error: Failed to get physical device surface formats.", res);
		return false;
	}

	if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
	{
		surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;
		surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else
	{
		surface_format = surface_formats[0];
	}

	m_surface_format = surface_format.format;

	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, VK_NULL_HANDLE);
	present_modes.resize(present_mode_count);
	res = vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, present_modes.data());
	if (res < 0)
	{
		ErrorMessage("Error: Failed to get physical device surface present modes.", res);
		return false;
	}

	for (auto pm : present_modes)
	{
		if(pm == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}

	VkSwapchainCreateInfoKHR swapchain_create_info{};
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.surface = m_surface;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageColorSpace = surface_format.colorSpace;
	swapchain_create_info.imageFormat = surface_format.format;
	swapchain_create_info.imageExtent = m_surface_extent;
	swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_create_info.minImageCount = image_count;
	swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;
	swapchain_create_info.presentMode = present_mode;
	swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	res = vkCreateSwapchainKHR(m_device, &swapchain_create_info, VK_NULL_HANDLE, &m_swapchain);
	if (res < 0)
	{
		ErrorMessage("Error: Failed to create swapchain.", res);
		return false;
	}

	uint32_t swapchain_image_count;
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_image_count, VK_NULL_HANDLE);
	m_swapchain_images.resize(swapchain_image_count);
	res = vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_image_count, m_swapchain_images.data());
	if (res < 0)
	{
		ErrorMessage("Error: Failed to get swapchain images.", res);
		return false;
	}

	VkImageViewCreateInfo image_view_create_info{};
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	image_view_create_info.format = m_surface_format;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount = 1;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount = 1;
	image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

	m_swapchain_image_views.resize(swapchain_image_count, VK_NULL_HANDLE);

	for (size_t i = 0; i < swapchain_image_count; i++)
	{
		image_view_create_info.image = m_swapchain_images[i];
		res = vkCreateImageView(m_device, &image_view_create_info, VK_NULL_HANDLE, &m_swapchain_image_views[i]);
		if (res < 0)
		{
			ErrorMessage("Error: Failed to create swapchain image views.", res);
			return false;
		}
	}


	return true;
}

const VkDevice& VulkanEngine::getDevice() const noexcept
{
	return m_device;
}

std::unique_ptr<VulkanWindow>& VulkanEngine::getWindow()
{
	return m_window;
}

Scene& VulkanEngine::getScene()
{
    return *m_scene;
}

const VkSwapchainKHR& VulkanEngine::getSwapchain() const noexcept
{
	return m_swapchain;
}

const std::vector<VkImageView>& VulkanEngine::getSwapchainImageViews() const noexcept
{
	return m_swapchain_image_views;
}

const std::vector<VkImage>& VulkanEngine::getSwapchainImages() const noexcept
{
	return m_swapchain_images;
}

const VkPhysicalDeviceMemoryProperties& VulkanEngine::getPhyDevMemProps() const noexcept
{
	return m_physical_device_memory_properties;
}

VkFormat VulkanEngine::getSurfaceFormat() const noexcept
{
	return m_surface_format;
}

VkExtent2D VulkanEngine::getSurfaceExtent() const noexcept
{
	return m_surface_extent;
}

uint32_t VulkanEngine::getQueueFamilyIndexGeneral()
{
	return m_queue_family_index_general;
}

uint32_t VulkanEngine::getQueueFamilyIndexTransfer()
{
	return m_queue_family_index_transfer;
}

void VulkanEngine::setScene(std::shared_ptr<Scene> scene)
{
	m_scene = scene;
}

void VulkanEngine::render()
{
	m_scene->render();
}

void VulkanEngine::run()
{
	if(m_scene.get() == nullptr)
		return;
	
    m_window->show();
	m_scene->getTimer().reset();

	while (m_running)
	{
        if(true)//check for events here???
		{
            QApplication::processEvents();

			if (!m_scene->getTimer().isPaused())
			{
				m_scene->getTimer().tick();
				
				render();
			}
		}
	};
}

#ifdef VK_USE_PLATFORM_WIN32_KHR

bool VulkanEngine::InitSurface()
{
	VkResult res;

	VkWin32SurfaceCreateInfoKHR surface_create_info{};
	surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_create_info.hinstance = m_window->getParams().hinstance;
    surface_create_info.hwnd = (HWND)m_window->winId();

	res = vkCreateWin32SurfaceKHR(m_instance, &surface_create_info, VK_NULL_HANDLE, &m_surface);
	if (res < 0)
	{
		ErrorMessage("Error: Failed to create Win32 surface.", res);
		return false;
	}

	VkBool32 support;
	res = vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, m_queue_family_index_general, m_surface, &support);
	if (res < 0)
	{
		ErrorMessage("Error: Failed to determine surface support.", res);
		return false;
	}

	if (support == VK_FALSE)
	{
		ErrorMessage("Error: WSI not supported by the physical device.");
		return false;
	}

	return true;
}

#elif defined(VK_USE_PLATFORM_XCB_KHR)

bool VulkanEngine::InitSurface()
{
	VkResult res;

	
	VkXcbSurfaceCreateInfoKHR surface_create_info{};
	surface_create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	surface_create_info.pNext = NULL;
	surface_create_info.flags = 0;
    surface_create_info.connection = QX11Info::connection();
    surface_create_info.window = m_window->winId();

	res = vkCreateXcbSurfaceKHR(m_instance, &surface_create_info, VK_NULL_HANDLE, &m_surface);
	if (res < 0)
	{
		ErrorMessage("Error: Failed to create XCB surface.", res);
		return false;
	}

	VkBool32 support;
	res = vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, m_queue_family_index_general, m_surface, &support);
	if (res < 0)
	{
		ErrorMessage("Error: Failed to determine surface support.", res);
		return false;
	}

	if (support == VK_FALSE)
	{
		ErrorMessage("Error: WSI not supported by the physical device.");
		return false;
	}

	return true;
}

#endif//VK_USE_PLATFORM_WIN32_KHR



///////////////////////////////////////////////////////////////////////////////////////////
////////////////////					D E B U G						///////////////////
///////////////////////////////////////////////////////////////////////////////////////////
#ifndef NDEBUG
VkBool32 VKAPI_CALL DebugCallback(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  objectType,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     messageCode,
	const char*                                 pLayerPrefix,
	const char*                                 pMessage,
	void*                                       pUserData)
{
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		std::string msg = "ERROR: \n";
		msg += pMessage;
		ErrorMessage(msg);
	}
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
	{
		std::string msg = "WARNING: \n";
		msg += pMessage;
		ErrorMessage(msg);
	}
	else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		std::string msg = "PERFORMANCE WARNING: \n";
		msg += pMessage;
		ErrorMessage(msg);
	}

	return false;
}

void VulkanEngine::InitDebug()
{
	PFN_vkCreateDebugReportCallbackEXT fpCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");

	VkDebugReportCallbackCreateInfoEXT debug_callback_create_info{};
	debug_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	debug_callback_create_info.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	debug_callback_create_info.pfnCallback = DebugCallback;

	fpCreateDebugReportCallback(m_instance, &debug_callback_create_info, VK_NULL_HANDLE, &debug_report);
}

void VulkanEngine::DeInitDebug()
{
	if (debug_report != VK_NULL_HANDLE)
	{
		PFN_vkDestroyDebugReportCallbackEXT fpDestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
		fpDestroyDebugReportCallback(m_instance, debug_report, VK_NULL_HANDLE);
		debug_report = VK_NULL_HANDLE;
	}
}

#else
void VulkanEngine::InitDebug() {}
void VulkanEngine::DeInitDebug() {}
#endif
