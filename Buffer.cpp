#include "Buffer.h"
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
    if (vkCreateBuffer(_context->GetDevice(), &bufferInfo, nullptr, &_buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_context->GetDevice(), _buffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = _context->FindMemoryType(memRequirements.memoryTypeBits, _properties);
    if (vkAllocateMemory(_context->GetDevice(), &allocInfo, nullptr, &_memory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
    vkBindBufferMemory(_context->GetDevice(), _buffer, _memory, 0);
    vkMapMemory(_context->GetDevice(), _memory, 0, size, 0, &_mappedMemory);
}
VulkanBuffer::~VulkanBuffer(){
    vkDestroyBuffer(_context->GetDevice(), _buffer, nullptr);   
    vkFreeMemory(_context->GetDevice(), _memory, nullptr);
}
void VulkanBuffer::SetData(void* pointer,size_t size){
    memcpy(_mappedMemory,pointer,size);
}
