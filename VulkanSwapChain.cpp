#include "VulkanSwapchain.h"
#include "VulkanContext.h"
#include <stdexcept>
#include <algorithm>
#include <array>
#include <limits>

VulkanSwapchain::VulkanSwapchain(VulkanContext& context) 
    : _context(context){
    init();
}

VulkanSwapchain::~VulkanSwapchain() {
    cleanup();
}

void VulkanSwapchain::init() {
    SwapChainSupportDetails details = _context.querySwapChainSupport();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(details.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(details.presentModes);
    VkExtent2D extent = chooseSwapExtent2D(details.capabilities,_context.getWindow());

    uint32_t imageCount = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
        imageCount = details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _context.getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices queueFamilyIndices = _context.getQueueFamilyIndices();
    std::array<uint32_t, 2> queueFamilyIndicesArray = { queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value() };
    if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndicesArray.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = details.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(_context.getDevice(), &createInfo, nullptr, &_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    _imageFormat = surfaceFormat.format;
    _extent = extent;

    // 获取交换链图像
    uint32_t count;
    vkGetSwapchainImagesKHR(_context.getDevice(), _swapchain, &count, nullptr);
    _images.resize(count);
    vkGetSwapchainImagesKHR(_context.getDevice(), _swapchain, &count, _images.data());

    // 创建图像视图
    _imageViews.resize(_images.size());
    for (size_t i = 0; i < _images.size(); i++) {
        _imageViews[i] = _context.createImageView(_images[i], _imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanSwapchain::cleanup() {
    for (auto imageView : _imageViews) {
        vkDestroyImageView(_context.getDevice(), imageView, nullptr);
    }
    vkDestroySwapchainKHR(_context.getDevice(), _swapchain, nullptr);
}

void VulkanSwapchain::recreate(VkExtent2D newExtent) {
    vkDeviceWaitIdle(_context.getDevice());
    cleanup();
    _extent = newExtent;
    init();
}

VkResult VulkanSwapchain::acquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex) {
    return vkAcquireNextImageKHR(_context.getDevice(), _swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, imageIndex);
}

VkResult VulkanSwapchain::present(VkQueue presentQueue, VkSemaphore waitSemaphore, uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    
    VkSwapchainKHR swapchains[] = {_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    return vkQueuePresentKHR(presentQueue, &presentInfo);
}