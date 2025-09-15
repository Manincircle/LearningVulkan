#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <array>
#define GLFW_INCLUDE_VULKAN
#include "glfw/glfw3.h"
#include "glm/glm.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/gtc/matrix_transform.hpp"
#include <chrono>
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/hash.hpp"
#include "stb_image.h"
#define PARTICLE_COUNT 3072
#define M_PI 3.14159265358979323846

#ifndef NDEBUG
    const bool enabledValidationLayer = true;
#else 
    const bool enabledValidationLayer = false;
#endif
#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H
const std::vector<const char*>LayerNames = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*>DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const int MAX_FRAMES_IN_FLIGHT = 3;

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,VkDevice device,uint32_t typeFilter, VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

// 调试回调函数类型
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

struct WindowInfo
{
    int width;
    int height;
    std::string title;
    WindowInfo(int w,int h,std::string t){
        width = w;
        height = h;
        title = t;
    }
};

struct VertexInfo
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(VertexInfo);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(VertexInfo, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(VertexInfo, color);
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(VertexInfo, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const VertexInfo& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct Particle{
    glm::vec4 position;
    glm::vec4 velocity;
    glm::vec4 color;
    
    static VkVertexInputBindingDescription GetBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Particle);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Particle, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Particle, velocity);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Particle, color);

        return attributeDescriptions;
    }
};

namespace std {
    template<>
    struct hash<VertexInfo> {
        size_t operator()(const VertexInfo& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                    (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                    (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

struct QueueFamily{
    std::optional<uint32_t>graphicsQueueFamily;
    std::optional<uint32_t>presentQueueFamily;
    bool isComplete(){
        return graphicsQueueFamily.has_value()&&presentQueueFamily.has_value();
    }
};

struct SwapChainSupportDetails{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class VulkanContext{
public:
    VulkanContext();
    VulkanContext(WindowInfo info);
    ~VulkanContext();
    GLFWwindow* GetWindow();
    void Run();
    VkInstance GetInstance() { return _instance; }
    VkSurfaceKHR GetSurface() { return _surface; }
    VkPhysicalDevice GetPhysicalDevice() { return _physicalDevice; }
    VkDevice GetDevice() { return _device; }
    VkSwapchainKHR GetSwapChain() { return _swapChain; }
    void InitVulkan();
    void InitWindow();
    void CreateInstance();
    void CreateSurface();
    void SetupDebugMessenger();
    void PickupPhysicalDevice();
    void CreateLogicDevice();
    void CreateSwapChain();
    void GetSwapChainImages(std::vector<VkImage>& images);
    void CreateImageViews();
    void CreateGraphicsPipeline();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CreateDescriptorSetLayout();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateTextureImage();
    void CreateTextureImageView();
    void CreateTextureSampler();
    void CreateDepthResources();
    void CreateColorResources();
    void CreateComputeBuffer();
    void CreateComputePipeline();
    void InitParticles();

    void CleanupAll();
    void CleanupWindow();
    void CleanupVulkan();
    void CleanupSwapChain();
    void CleanupCompute();
    void DestroyDebugMessenger();

    void RecreateSwapChain();

    bool IsDeviceSuitable(VkPhysicalDevice device);
    bool CheckExtensionSupport(VkPhysicalDevice device);
    QueueFamily FindQueueFamilyIndex(VkPhysicalDevice device);
    SwapChainSupportDetails GetSwapChainSupportDetails(VkPhysicalDevice device);
    VkSurfaceCapabilitiesKHR GetSwapChainCapabilities(VkPhysicalDevice device);
    std::vector<VkSurfaceFormatKHR> GetSwapChainFormat(VkPhysicalDevice device);
    std::vector<VkPresentModeKHR> GetSwapChainPresentMode(VkPhysicalDevice device);
    VkSurfaceFormatKHR ChooseSurfaceFMT(const SwapChainSupportDetails& swapchain);
    VkPresentModeKHR ChoosePresentMode(const SwapChainSupportDetails& swapchain);
    VkExtent2D ChooseExtent2D(const SwapChainSupportDetails& swapchain);
    std::vector<char> ReadFile(const std::string& filePath);
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    void BeginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags);
    void EndCommandBuffer(VkCommandBuffer commandBuffer);
    void RecordCommandBuffer(VkCommandBuffer commandBuffer,uint32_t imageIndex);
    void RecordComputeCommandBuffer(VkCommandBuffer commandBuffer);
    void DrawFrame();
    void DrawCompute();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void CopyBuffer(VkBuffer src,VkBuffer dst,VkDeviceSize size);
    void UpdateUniformBuffer(uint32_t currentImage);
    void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkCommandBuffer BeginSingleTimeCommands();
    void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkImageView CreateImageView(VkImage image, VkFormat format,uint32_t mipLevels, VkImageAspectFlags aspectFlags);
    void TransitionImageLayout(VkImage image, VkFormat format, uint32_t mipLevels, VkImageLayout oldLayout, VkImageLayout newLayout);
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat FindDepthFormat();
    void LoadModel();
    VkSampleCountFlagBits GetMaxUsableSampleCount();
    void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    void UpdateComputeDescriptors(uint32_t currentFrame);
    private:
    std::vector<VertexInfo> _vertices;
    std::vector<uint32_t> _indices;
    WindowInfo _info{1280,800,"Vulkan Test"};
    GLFWwindow* _window = nullptr;
    VkInstance _instance;
    VkPhysicalDevice _physicalDevice;
    VkDevice _device;
    VkSurfaceKHR _surface;
    VkDebugUtilsMessengerEXT _debugMessenger;
    VkQueue _graphicsQueue;
    VkQueue _presentQueue;
    VkQueue _computeQueue;
    VkSwapchainKHR _swapChain;
    VkFormat _swapChainImageFMT;
    VkExtent2D _swapChainImageExtent;
    std::vector<VkImage> _swapChainImages;
    std::vector<VkImageView> _imageViews;
    VkDescriptorSetLayout _descriptorSetLayout;
    VkPipeline _pipeline;
    VkPipelineLayout _pipelineLayout;
    QueueFamily _queueFamily;
    std::vector<VkCommandBuffer> _commandBuffers;
    std::vector<VkCommandBuffer> _computeCommandBuffers;
    VkCommandPool _commandPool;
    std::vector<VkSemaphore> _imageAvailableSemaphores;
    std::vector<VkSemaphore> _renderFinishedSemaphores;
    std::vector<VkSemaphore> _computeFinishedSemaphores;
    std::vector<VkFence> _inFlightFences;
    std::vector<VkFence> _computeFinishedFences;
    uint32_t currentFrame = 0;
    VkBuffer _vertexBuffer;
    VkDeviceMemory _vertexBufferMemory;
    VkBuffer _indexBuffer;
    VkDeviceMemory _indexBufferMemory;
    std::vector<VkBuffer> _uniformBuffers;
    std::vector<VkDeviceMemory> _uniformBuffersMemory;
    std::vector<VkBuffer> _uniformcomputeBuffers;
    std::vector<VkDeviceMemory> _uniformcomputeBuffersMemory;
    std::vector<void*>_uniformBuffersMapped;
    std::vector<void*>_uniformcomputeBuffersMapped;
    VkDescriptorPool _descriptorPool;
    std::vector<VkDescriptorSet> _descriptorSets;
    uint32_t _mipLevels;
    VkImage _textureImage;
    VkDeviceMemory _textureImageMemory;
    VkImageView _textureImageView;
    VkSampler _textureSampler;
    VkImage _depthImage;
    VkDeviceMemory _depthImageMemory;
    VkImageView _depthImageView;
    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkImage _colorImage;
    VkDeviceMemory _colorImageMemory;
    VkImageView _colorImageView;
    std::vector<VkBuffer> _computeBuffers;
    std::vector<VkDeviceMemory> _computeBuffersMemorys;
    VkPipeline _computePipeline;
    VkPipelineLayout _computePipelineLayout;
    VkDescriptorSetLayout _computeDescriptorSetLayout;
    VkDescriptorPool _computeDescriptorPool;
    std::vector<VkDescriptorSet> _computeDescriptorSets;
    std::chrono::high_resolution_clock::time_point _lastFrameTime;
    float _deltaTime = 0.0f;
public:
    bool framebufferResized = false;
    
};
#endif