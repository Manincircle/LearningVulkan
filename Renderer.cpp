#include "Renderer.h"
#include "VulkanContext.h"
#include <stdexcept>

Renderer::Renderer(VulkanContext& context, uint32_t maxFramesInFlight)
    : _context(context), _maxFramesInFlight(maxFramesInFlight) {
    
    // 创建命令池
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = _context.getQueueFamilyIndices().graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(_context.getDevice(), &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
    
    // 创建命令缓冲区
    _commandBuffers.resize(_maxFramesInFlight);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = _maxFramesInFlight;
    if (vkAllocateCommandBuffers(_context.getDevice(), &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // 创建同步对象
    _imageAvailableSemaphores.resize(_maxFramesInFlight);
    _renderFinishedSemaphores.resize(_maxFramesInFlight);
    _inFlightFences.resize(_maxFramesInFlight);
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    for (uint32_t i = 0; i < _maxFramesInFlight; ++i) {
        vkCreateSemaphore(_context.getDevice(), &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]);
        vkCreateSemaphore(_context.getDevice(), &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]);
        vkCreateFence(_context.getDevice(), &fenceInfo, nullptr, &_inFlightFences[i]);
    }
}

Renderer::~Renderer() {
    for (uint32_t i = 0; i < _maxFramesInFlight; ++i) {
        vkDestroySemaphore(_context.getDevice(), _imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(_context.getDevice(), _renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(_context.getDevice(), _inFlightFences[i], nullptr);
    }
    vkDestroyCommandPool(_context.getDevice(), _commandPool, nullptr);
}

VkCommandBuffer Renderer::beginFrame(VulkanSwapchain& swapchain) {
    vkWaitForFences(_context.getDevice(), 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = swapchain.acquireNextImage(_imageAvailableSemaphores[_currentFrame], &_imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // ... 通知上层进行重建 ...
        return nullptr;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(_context.getDevice(), 1, &_inFlightFences[_currentFrame]);
    
    VkCommandBuffer commandBuffer = _commandBuffers[_currentFrame];
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    return commandBuffer;
}

void Renderer::endFrame(VulkanSwapchain& swapchain,VkQueue graphicsQueue,VkQueue presentQueue) {
    VkCommandBuffer commandBuffer = _commandBuffers[_currentFrame];
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {_imageAvailableSemaphores[_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkResult result = swapchain.present(presentQueue, _renderFinishedSemaphores[_currentFrame], _imageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
       // ... 通知上层进行重建 ...
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }
    
    _currentFrame = (_currentFrame + 1) % _maxFramesInFlight;
}