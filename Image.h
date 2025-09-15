#ifndef IMAGE_H
#define IMAGE_H

#include "VulkanContext.h" // 包含VulkanContext以获取设备句柄等
#include <string>
#include <memory> // 用于 std::unique_ptr

class VulkanImage {
private:
    VulkanContext* _context;
    VkImage _image = VK_NULL_HANDLE;
    VkDeviceMemory _memory = VK_NULL_HANDLE;
    VkImageView _view = VK_NULL_HANDLE;
    VkSampler _sampler = VK_NULL_HANDLE;

    VkFormat _format;
    VkExtent3D _extent;
    VkImageLayout _layout;
    uint32_t _mipLevels;

    // 私有构造函数，强制使用静态工厂函数创建实例
    VulkanImage(VulkanContext& context, VkFormat format, VkExtent3D extent, uint32_t mipLevels, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

public:
    ~VulkanImage();

    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    // --- 静态工厂函数 ---
    
    // 用于从文件加载纹理，会自动处理暂存、拷贝、mip-mapping和采样器创建
    static std::unique_ptr<VulkanImage> createTextureFromFile(VulkanContext& context, const std::string& path);

    // 用于创建2D图像，例如颜色附件、深度缓冲、存储图像等
    static std::unique_ptr<VulkanImage> create2DImage(VulkanContext& context, VkExtent2D extent, VkFormat format, 
                                                    VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags);

    // --- 成员函数 ---

    // 转换图像布局
    void transitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout);

    // --- Getters ---
    VkImage getImage() const { return _image; }
    VkImageView getView() const { return _view; }
    VkSampler getSampler() const { return _sampler; }
    VkFormat getFormat() const { return _format; }
    VkExtent2D getExtent2D() const { return {_extent.width, _extent.height}; }
    VkImageLayout getLayout() const { return _layout; }

private:
    // 内部辅助函数
    void createImageView(VkImageAspectFlags aspectFlags);
    void createSampler();
    void generateMipmaps(VkCommandBuffer cmd);
};

#endif