#include "shader.h"
#include "engine.h"
#include "debug.h"
#include <fstream>

Shader::Shader(std::string pathname)
{
	VkResult res;
	
	//open file and go to the end
	std::ifstream file(pathname, std::ios::binary | std::ios::ate);
	//get end position to get file size
	std::streamsize size = file.tellg();
	//go back to beginning
	file.seekg(0, std::ios::beg);
	
	std::vector<char> buffer(size);
	file.read(buffer.data(), size);
	
	VkShaderModuleCreateInfo shader_modul_create_info{};
	shader_modul_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_modul_create_info.flags = 0;
	shader_modul_create_info.pNext = NULL;
	shader_modul_create_info.codeSize = size;
	shader_modul_create_info.pCode = (uint32_t*)buffer.data();
	
	res = vkCreateShaderModule(VulkanEngine::get().getDevice(), &shader_modul_create_info, VK_NULL_HANDLE, &m_module);
	if( res < 0)
	{
		ErrorMessage("Failed to create a shader module.", res);
		return;
	}
}

Shader::~Shader()
{
	if(m_module != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(VulkanEngine::get().getDevice(), m_module, VK_NULL_HANDLE);
		m_module = VK_NULL_HANDLE;
	}
}

VkShaderModule Shader::getModule()
{
	return m_module;
}
