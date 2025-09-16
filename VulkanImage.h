#pragma once
#include "VulkanContext.h"
#include <string>
#include <memory>

// 前向声明，以避免在头文件中包含 ImmediateSubmitter.h
class ImmediateSubmitter;

class VulkanImage {
public:
    // --- 静态工厂函数 (签名已更新) ---
    
    // 从文件加载纹理
    static std::unique_ptr<VulkanImage> createTextureFromFile(
        VulkanContext& context, 
        ImmediateSubmitter& uploader, 
        const std::string& path
    );

    // 创建通用的2D图像（如颜色/深度附件）
    static std::unique_ptr<VulkanImage> create2DImage(
        VulkanContext& context, 
        VkExtent2D extent, 
        VkFormat format, 
        VkImageUsageFlags usage, 
        VkMemoryPropertyFlags properties, 
        VkImageAspectFlags aspectFlags
    );

    ~VulkanImage();

    // 禁止拷贝
    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    // --- 成员函数 ---

    // 在给定的命令缓冲区中记录一个布局转换命令
    // 这个函数现在是公开的，以便更灵活地在渲染循环中使用
    void recordTransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout);

    // --- Getters ---
    VkImage getImage() const { return _image; }
    VkImageView getView() const { return _view; }
    VkSampler getSampler() const { return _sampler; }
    VkFormat getFormat() const { return _format; }
    VkExtent2D getExtent2D() const { return {_extent.width, _extent.height}; }
    VkImageLayout getLayout() const { return _layout; }
    VkDescriptorImageInfo GetDescriptorInfo() { return {_sampler, _view, _layout}; }

private:
    // 构造函数保持私有，强制使用工厂函数
    VulkanImage(VulkanContext& context, VkFormat format, VkExtent3D extent, uint32_t mipLevels, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    
    // 内部辅助函数
    void createImageView(VkImageAspectFlags aspectFlags);
    void createSampler();
    void recordGenerateMipmaps(VkCommandBuffer cmd); // 从 generateMipmaps 改名为 recordGenerateMipmaps

    VulkanContext& _context;
    VkImage _image = VK_NULL_HANDLE;
    VkDeviceMemory _memory = VK_NULL_HANDLE;
    VkImageView _view = VK_NULL_HANDLE;
    VkSampler _sampler = VK_NULL_HANDLE;

    VkFormat _format;
    VkExtent3D _extent;
    VkImageLayout _layout;
    uint32_t _mipLevels;
};