#ifndef BUFFER_H
#define BUFFER_H
#include "VulkanContext.h"
#include <cstddef>
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
    void SetData(void* pointer,size_t size);
    void* GetMappedMemory() { return _mappedMemory; }
    VkBuffer GetBuffer() { return _buffer; }
    VkDeviceMemory GetMemory() { return _memory; }
    VkDeviceSize GetSize() { return _size; }
    VkBufferUsageFlags GetUsage() { return _usage; }
    VkMemoryPropertyFlags GetProperties() { return _properties; }
};
#endif