#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include "VulkanContext.h"
#include <string>
#include <vector>
#include <memory>

// 前向声明，避免循环包含
class ImmediateSubmitter;
class VulkanBuffer;

class VulkanImage {
public:
    // --- 静态工厂函数 ---

    // 创建标准 2D 纹理，可选择是否生成 Mipmaps
    static std::unique_ptr<VulkanImage> createTexture(
        VulkanContext& context,
        ImmediateSubmitter& uploader,
        const std::string& path,
        bool generateMipmaps = true,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB
    );

    // 创建立方体贴图 (天空盒)
    static std::unique_ptr<VulkanImage> createCubemap(
        VulkanContext& context,
        ImmediateSubmitter& uploader,
        const std::string& path,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB
    );

    // 创建渲染附件 (颜色/深度)
    static std::unique_ptr<VulkanImage> createAttachment(
        VulkanContext& context,
        VkExtent2D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT
    );

    ~VulkanImage();

    // 禁止拷贝
    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;


    // --- 成员函数 ---

    // 在给定的命令缓冲区中记录一个布局转换命令
    void recordTransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout);

    // --- Getters ---
    VkImage getImage() const { return _image; }
    VkImageView getView() const { return _view; }
    VkSampler getSampler() const { return _sampler; }
    VkFormat getFormat() const { return _format; }
    VkExtent2D getExtent() const { return _extent; }
    VkImageLayout getLayout() const { return _layout; }
    uint32_t getMipLevels() const { return _mipLevels; }
    VkSampleCountFlagBits getSampleCount() const { return _msaaCount; }

    // 获取用于更新描述符集的信息
    VkDescriptorImageInfo getDescriptorInfo() const;

    // 统一的私有构造函数
    VulkanImage(
        VulkanContext& context,
        VkExtent2D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        uint32_t mipLevels,
        VkSampleCountFlagBits numSamples,
        VkImageCreateFlags createFlags,
        uint32_t arrayLayers
    );

private:

    // 内部辅助函数
    void createImageView(VkImageAspectFlags aspectFlags, VkImageViewType viewType);
    void createSampler();
    void recordGenerateMipmaps(VkCommandBuffer cmd);

    VulkanContext* _context; // 指针，因为 Image 可能被移动
    VkImage _image = VK_NULL_HANDLE;
    VkDeviceMemory _memory = VK_NULL_HANDLE;
    VkImageView _view = VK_NULL_HANDLE;
    VkSampler _sampler = VK_NULL_HANDLE;

    VkFormat _format;
    VkExtent2D _extent;
    VkImageLayout _layout;
    uint32_t _mipLevels;
    VkSampleCountFlagBits _msaaCount;
    VkImageCreateFlags _imageCreateFlags;
};

#endif // VULKAN_IMAGE_H