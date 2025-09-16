#include "VulkanBuffer.h"
#include <stdexcept>
VulkanBuffer::VulkanBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties){
    this->_size = size;
    this->_usage = usage;
    this->_properties = properties;
    this->_context = &context;
    this->_mappedMemory = nullptr;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(_context->getDevice(), &bufferInfo, nullptr, &_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_context->getDevice(), _buffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = _context->findMemoryType(memRequirements.memoryTypeBits, _properties);
    if (vkAllocateMemory(_context->getDevice(), &allocInfo, nullptr, &_memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
    vkBindBufferMemory(_context->getDevice(), _buffer, _memory, 0);
    if(usage & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) vkMapMemory(_context->getDevice(), _memory, 0, size, 0, &_mappedMemory);
}
VulkanBuffer::~VulkanBuffer(){
    if (_mappedMemory) {
        vkUnmapMemory(_context->getDevice(), _memory);
    }
    vkDestroyBuffer(_context->getDevice(), _buffer, nullptr);   
    vkFreeMemory(_context->getDevice(), _memory, nullptr);
}
void VulkanBuffer::SetData(void* pointer,size_t size){
    memcpy(_mappedMemory,pointer,size);
}