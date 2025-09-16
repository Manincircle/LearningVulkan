#pragma once
#include "VulkanContext.h"
#include <cstddef>
#include <vulkan/vulkan.h>
class VulkanBuffer{
private:
    VkBuffer _buffer;
    VkDeviceMemory _memory;
    VkDeviceSize _size;
    VkBufferUsageFlags _usage;
    VkMemoryPropertyFlags _properties;
    void* _mappedMemory;
    VulkanContext* _context;
public:
    VulkanBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    ~VulkanBuffer();
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    void SetData(void* pointer,size_t size);
    void* GetMappedMemory() { return _mappedMemory; }
    VkBuffer GetBuffer() const { return _buffer; }
    VkDeviceMemory GetMemory() { return _memory; }
    VkDeviceSize GetSize() { return _size; }
    VkBufferUsageFlags GetUsage() { return _usage; }
    VkMemoryPropertyFlags GetProperties() { return _properties; }
    VkDescriptorBufferInfo GetDescriptorInfo() { return {_buffer,0,_size}; }
};