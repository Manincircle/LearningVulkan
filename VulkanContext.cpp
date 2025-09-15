#include "VulkanContext.h"
#include "glfw/glfw3.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <set>
#include <fstream>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <random>

// Helper function to check extension support
bool checkExtensionSupport(const std::vector<VkExtensionProperties>& availableExtensions, const char* extensionName) {
    for (const auto& extension : availableExtensions) {
        if (strcmp(extension.extensionName, extensionName) == 0) {
            return true;
        }
    }
    return false;
}

bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VulkanContext::VulkanContext(){
    InitWindow();
    InitVulkan();
}

VulkanContext::~VulkanContext(){
    CleanupAll();
    std::cout<<"[INFO] Program ended!"<<std::endl;
}

VulkanContext::VulkanContext(WindowInfo info){
    _info = info;
    InitWindow();
    InitVulkan();
}
void VulkanContext::InitWindow(){
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    _window = glfwCreateWindow(_info.width, _info.height, _info.title.c_str(), nullptr, nullptr);
    if (!_window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
    }
    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
}

void VulkanContext::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<VulkanContext*>(glfwGetWindowUserPointer(window));
    app->_info.width = width;
    app->_info.height = height;
    app->framebufferResized = true;
}

void VulkanContext::LoadModel(){
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, "model.obj")) {
        throw std::runtime_error(err);
    }
    std::unordered_map<VertexInfo, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            VertexInfo vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            double scale = 0.009f;
            vertex.pos.x *= scale;
            vertex.pos.y *= scale;
            vertex.pos.z *= scale;

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(_vertices.size());
                _vertices.push_back(vertex);
            }
            _indices.push_back(uniqueVertices[vertex]);
        }
    }
}

void VulkanContext::InitVulkan() {
    LoadModel();
   CreateInstance();
   CreateSurface();
   PickupPhysicalDevice();
   CreateLogicDevice();
   CreateSwapChain();
   GetSwapChainImages(_swapChainImages);
   CreateImageViews();
   CreateCommandPool();
   CreateColorResources();
   CreateDepthResources();
   CreateUniformBuffers();
   CreateDescriptorSetLayout();
   CreateTextureImage();
   CreateTextureImageView();
   CreateTextureSampler();
   CreateDescriptorPool();
   CreateDescriptorSets();
   CreateGraphicsPipeline();
   InitParticles();
   CreateComputePipeline();
   CreateVertexBuffer();
   CreateIndexBuffer();
   CreateCommandBuffers();
   CreateSyncObjects();
}

void VulkanContext::CreateInstance(){
     VkApplicationInfo appInfo{};
     appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
     appInfo.pApplicationName = "VulkanVulkanContext";
     appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
     appInfo.pEngineName = "NoEngine";
     appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
     appInfo.apiVersion = VK_API_VERSION_1_3;
 
     uint32_t glfwExtensionCount = 0;
     const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
     
     std::vector<const char*> extensions;
     if (glfwExtensions != nullptr) {
         extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
     }
     if (enabledValidationLayer) {
         extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
     }
     
     // Check extension support
     uint32_t availableExtensionCount = 0;
     vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
     std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
     vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());
     
     // Verify all required extensions are supported
     for (const auto& extension : extensions) {
         if (!checkExtensionSupport(availableExtensions, extension)) {
             std::cerr << "Extension " << extension << " not supported!" << std::endl;
         }
     }
 
     // Vulkan instance creation info
     VkInstanceCreateInfo createInfo{};
     createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
     createInfo.pApplicationInfo = &appInfo;
     createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
     createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
     
     // Enable validation layers based on compilation mode
     if (enabledValidationLayer) {
         createInfo.enabledLayerCount = static_cast<uint32_t>(LayerNames.size());
         createInfo.ppEnabledLayerNames = LayerNames.data();
     } else {
         createInfo.enabledLayerCount = 0;
         createInfo.ppEnabledLayerNames = nullptr;
     }
 
     // Create Vulkan instance
     VkResult result = vkCreateInstance(&createInfo, nullptr, &_instance);
     if (result != VK_SUCCESS) {
         switch (result) {
             case VK_ERROR_OUT_OF_HOST_MEMORY:
                 std::cerr << "Failed to create Vulkan instance: Out of host memory!" << std::endl; break;
             case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                 std::cerr << "Failed to create Vulkan instance: Out of device memory!" << std::endl; break;
             case VK_ERROR_INITIALIZATION_FAILED:
                 std::cerr << "Failed to create Vulkan instance: Initialization failed!" << std::endl; break;
             case VK_ERROR_EXTENSION_NOT_PRESENT:
                 std::cerr << "Failed to create Vulkan instance: Required extension not present!" << std::endl; break;
             case VK_ERROR_LAYER_NOT_PRESENT:
                 std::cerr << "Failed to create Vulkan instance: Required layer not present!" << std::endl; break;
             default:
                 std::cerr << "Failed to create Vulkan instance! Unknown error code: " << result << std::endl; break;
         }
         glfwDestroyWindow(_window);
         glfwTerminate();
         return;
     }
 
     std::cout << "[SUCCESS] Vulkan instance created" << std::endl;
     
     // Setup debug messenger
     if (enabledValidationLayer) {
         SetupDebugMessenger();
     }
}

void VulkanContext::CreateSurface() {
    VkResult result = glfwCreateWindowSurface(_instance, _window, nullptr, &_surface);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create window surface!" << std::endl;
        glfwDestroyWindow(_window);
        glfwTerminate();
        return;
    }
    std::cout << "[SUCCESS] Window surface created" << std::endl;
}

bool VulkanContext::CheckExtensionSupport(VkPhysicalDevice device){
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> properties(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, properties.data());
    std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());
    for(const auto& extension : properties){
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}   

// Debug callback function implementation
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    (void)pUserData; // Suppress unused parameter warning
    
    // Choose output method based on message severity
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Vulkan Validation Layer: ";
        
        // Add prefix based on message type
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            std::cerr << "[General] ";
        }
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            std::cerr << "[Validation] ";
        }
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            std::cerr << "[Performance] ";
        }
        
        // Add color identifier based on severity
        switch (messageSeverity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                std::cerr << "[ERROR] ";
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                std::cerr << "[WARNING] ";
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                std::cerr << "[INFO] ";
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                std::cerr << "[VERBOSE] ";
                break;
            default:
                std::cerr << "[UNKNOWN] ";
                break;
        }
        
        std::cerr << pCallbackData->pMessage << std::endl;
    }
    
    return VK_FALSE; // Don't interrupt program execution
}

void VulkanContext::SetupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
    
    // Get function pointer for creating debug messenger
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        VkResult result = func(_instance, &createInfo, nullptr, &_debugMessenger);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create debug messenger!" << std::endl;
        } else {
            std::cout << "[SUCCESS] Debug messenger created" << std::endl;
        }
    } else {
        std::cerr << "Debug messenger extension not available!" << std::endl;
    }
}

void VulkanContext::CreateSwapChain(){
    SwapChainSupportDetails details = GetSwapChainSupportDetails(_physicalDevice);

    VkSurfaceFormatKHR FMT = ChooseSurfaceFMT(details);
    VkPresentModeKHR presentMode = ChoosePresentMode(details);
    VkExtent2D extent = ChooseExtent2D(details);

    uint32_t imageCount = details.capabilities.minImageCount+1;
    if(details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount){
        imageCount = details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = _surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = FMT.format;
    swapchainCreateInfo.imageColorSpace = FMT.colorSpace;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {_queueFamily.graphicsQueueFamily.value(),_queueFamily.presentQueueFamily.value()};
    if(_queueFamily.graphicsQueueFamily != _queueFamily.presentQueueFamily){
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else{
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.queueFamilyIndexCount = 0;
        swapchainCreateInfo.pQueueFamilyIndices = nullptr;
    }
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.preTransform = details.capabilities.currentTransform;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    swapchainCreateInfo.presentMode = presentMode;
         VkResult result = vkCreateSwapchainKHR(_device,&swapchainCreateInfo,nullptr,&_swapChain);
     if(result != VK_SUCCESS){
         throw std::runtime_error("Failed to create swap chain!");
     }
     _swapChainImageFMT = FMT.format;
     _swapChainImageExtent = extent;
     std::cout << "[SUCCESS] Swap chain created" << std::endl;
}

void VulkanContext::GetSwapChainImages(std::vector<VkImage>& images){
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(_device,_swapChain,&imageCount,nullptr);
    if(imageCount!=0){
        images.resize(imageCount);
        vkGetSwapchainImagesKHR(_device,_swapChain,&imageCount,images.data());
    }
     std::cout << "[SUCCESS] Swap chain images created" << std::endl;
}

void VulkanContext::CreateImageViews(){
    int size = _swapChainImages.size();
    _imageViews.resize(size);
    for(int i = 0;i < size;i++){
        _imageViews[i] = CreateImageView(_swapChainImages[i], _swapChainImageFMT, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    std::cout << "[SUCCESS] Image view created" << std::endl;
}

void VulkanContext::CreateGraphicsPipeline(){

    VkShaderModule vert = CreateShaderModule(ReadFile("vert.spv"));
    VkShaderModule frag = CreateShaderModule(ReadFile("frag.spv"));

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.module = vert;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.pName = "VSMain";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.module = frag;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.pName = "PSMain";

    VkPipelineShaderStageCreateInfo ShaderStageInfos[] = {vertStageInfo,fragStageInfo};

    auto bindingDescription = Particle::GetBindingDescription();
    auto attributeDescriptions = Particle::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = _msaaSamples;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayout;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo PDSCreateInfo{};
    PDSCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    PDSCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    PDSCreateInfo.pDynamicStates = dynamicStates.data();

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    VkPipelineRenderingCreateInfo renderingCreateInfo{};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = 1;
    renderingCreateInfo.pColorAttachmentFormats = &_swapChainImageFMT;
    renderingCreateInfo.depthAttachmentFormat = FindDepthFormat();

    VkGraphicsPipelineCreateInfo GPCreateInfo{};
    GPCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    GPCreateInfo.stageCount = 2;
    GPCreateInfo.pStages = ShaderStageInfos;
    GPCreateInfo.pVertexInputState   = &vertexInputInfo;
    GPCreateInfo.pInputAssemblyState = &inputAssembly;
    GPCreateInfo.pViewportState      = &viewportState;
    GPCreateInfo.pRasterizationState = &rasterizer;
    GPCreateInfo.pMultisampleState   = &multisampling;
    GPCreateInfo.pDepthStencilState  = nullptr;
    GPCreateInfo.pColorBlendState    = &colorBlending;
    GPCreateInfo.pDynamicState       = &PDSCreateInfo;
    GPCreateInfo.layout              = _pipelineLayout;
    GPCreateInfo.renderPass          = VK_NULL_HANDLE;
    GPCreateInfo.subpass             = 0;
    GPCreateInfo.basePipelineHandle  = VK_NULL_HANDLE;
    GPCreateInfo.pDepthStencilState  = &depthStencil;
    GPCreateInfo.pNext               = &renderingCreateInfo;

    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &GPCreateInfo, nullptr, &_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(_device,vert,nullptr);
    vkDestroyShaderModule(_device,frag,nullptr);

    std::cout << "[SUCCESS] Graphics pipeline created" << std::endl;
}

void VulkanContext::CreateCommandBuffers(){
    _commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    _computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();
    if (vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
    VkCommandBufferAllocateInfo allocInfoCompute{};
    allocInfoCompute.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfoCompute.commandPool = _commandPool;
    allocInfoCompute.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfoCompute.commandBufferCount = (uint32_t)_computeCommandBuffers.size();
    if (vkAllocateCommandBuffers(_device, &allocInfoCompute, _computeCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
    std::cout << "[SUCCESS] Command buffers created" << std::endl;
}

void VulkanContext::CreateCommandPool(){
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = _queueFamily.graphicsQueueFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }
    std::cout << "[SUCCESS] Command pool created" << std::endl;
}

void VulkanContext::CreateSyncObjects(){
    _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    _computeFinishedFences.resize(MAX_FRAMES_IN_FLIGHT);
    _computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(_device, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS ||
            vkCreateFence(_device, &fenceInfo, nullptr, &_computeFinishedFences[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_computeFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
    std::cout << "[SUCCESS] Synchronization objects created" << std::endl;
}

void VulkanContext::CreateDescriptorSetLayout(){
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
    std::cout << "[SUCCESS] Descriptor set layout created" << std::endl;
}

void VulkanContext::DestroyDebugMessenger() {
    if (enabledValidationLayer) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(_instance, _debugMessenger, nullptr);
        }
    }
}

void VulkanContext::PickupPhysicalDevice(){
    uint32_t DeviceCount;
    vkEnumeratePhysicalDevices(_instance,&DeviceCount,nullptr);
    if(DeviceCount == 0){
        throw std::runtime_error("Failed to enumerate physical device");
    }
    std::vector<VkPhysicalDevice>devices(DeviceCount);
    vkEnumeratePhysicalDevices(_instance,&DeviceCount,devices.data());

    for(const auto& d:devices){
        if(IsDeviceSuitable(d)){
            _physicalDevice = d;
            _msaaSamples = GetMaxUsableSampleCount();
            break;
        }
    }

    if(_physicalDevice==nullptr){
        throw std::runtime_error("Failed to find suitable physical device");
    }
    std::cout << "[SUCCESS] Physical device selected" << std::endl;
    std::cout << "[SUCCESS] Physical device enumeration completed" << std::endl;
}

void VulkanContext::CreateComputeBuffer(){
    _computeBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    _computeBuffersMemorys.resize(MAX_FRAMES_IN_FLIGHT);
    VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
    {
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _computeBuffers[i], _computeBuffersMemorys[i]);
    }
}

void VulkanContext::InitParticles(){
    CreateComputeBuffer();
    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

    std::vector<Particle> particles(PARTICLE_COUNT);

    for (auto& particle : particles) {
        float r = 0.25f * cbrt(rndDist(rndEngine));

    // 随机角度
        float theta = rndDist(rndEngine) * 2.0f * M_PI; // 方位角
        float phi   = acos(1.0f - 2.0f * rndDist(rndEngine)); // 极角

    // 三维坐标 (球坐标转笛卡尔)
        float x = r * sin(phi) * cos(theta);
        float y = r * sin(phi) * sin(theta);
        float z = r * cos(phi);

        particle.position = glm::vec4(x, y, z, 1.0f);

    // 初始速度 = 归一化方向 * 很小的速度系数
        particle.velocity = glm::vec4(glm::normalize(glm::vec3(x, y, z)) * 0.0005f, 0.0f);

    // 随机颜色
        particle.color = glm::vec4(rndDist(rndEngine),rndDist(rndEngine),rndDist(rndEngine),1.0f);
    }
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, particles.data(), (size_t)bufferSize);
    vkUnmapMemory(_device, stagingBufferMemory);
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        CopyBuffer(stagingBuffer, _computeBuffers[i], bufferSize);
    }
    vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
    std::cout << "[SUCCESS] Particles initialized" << std::endl;
}

void VulkanContext::CreateComputePipeline(){
    std::array<VkDescriptorSetLayoutBinding, 3> layoutBindings{};
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].pImmutableSamplers = nullptr;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[1].pImmutableSamplers = nullptr;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[2].pImmutableSamplers = nullptr;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(_device, &layoutInfo, nullptr, &_computeDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute descriptor set layout!");
    }
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2) } // 输入输出
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_computeDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, _computeDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _computeDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    _computeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(_device, &allocInfo, _computeDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate compute descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo uniformBufferInfo{};
        uniformBufferInfo.buffer = _uniformcomputeBuffers[i];
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range = sizeof(float);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

        VkDescriptorBufferInfo storageBufferInfoLastFrame{};
        storageBufferInfoLastFrame.buffer = _computeBuffers[(i - 1 + MAX_FRAMES_IN_FLIGHT) % MAX_FRAMES_IN_FLIGHT];
        storageBufferInfoLastFrame.offset = 0;
        storageBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = _computeDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;


        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = _computeDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &storageBufferInfoLastFrame;

        VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
        storageBufferInfoCurrentFrame.buffer = _computeBuffers[i];
        storageBufferInfoCurrentFrame.offset = 0;
        storageBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = _computeDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &storageBufferInfoCurrentFrame;

        vkUpdateDescriptorSets(_device, 3, descriptorWrites.data(), 0, nullptr);
    }

    auto computeShaderCode = ReadFile("compute.spv");

    VkShaderModule computeShaderModule = CreateShaderModule(computeShaderCode);

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "CSMain";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &_computeDescriptorSetLayout;

    if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_computePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = _computePipelineLayout;
    pipelineInfo.stage = computeShaderStageInfo;

    if (vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }
    vkDestroyShaderModule(_device, computeShaderModule, nullptr);
}

void VulkanContext::UpdateComputeDescriptors(uint32_t currentFrame) {
    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = _uniformcomputeBuffers[currentFrame];
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(float); // time step or so

    VkDescriptorBufferInfo storageBufferInfoLastFrame{};
    storageBufferInfoLastFrame.buffer = _computeBuffers[(currentFrame - 1 + MAX_FRAMES_IN_FLIGHT) % MAX_FRAMES_IN_FLIGHT];
    storageBufferInfoLastFrame.offset = 0;
    storageBufferInfoLastFrame.range = sizeof(Particle) * PARTICLE_COUNT;

    VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
    storageBufferInfoCurrentFrame.buffer = _computeBuffers[currentFrame];
    storageBufferInfoCurrentFrame.offset = 0;
    storageBufferInfoCurrentFrame.range = sizeof(Particle) * PARTICLE_COUNT;

    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    descriptorWrites[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrites[0].dstSet = _computeDescriptorSets[currentFrame];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

    descriptorWrites[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrites[1].dstSet = _computeDescriptorSets[currentFrame];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &storageBufferInfoLastFrame;

    descriptorWrites[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrites[2].dstSet = _computeDescriptorSets[currentFrame];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &storageBufferInfoCurrentFrame;

    vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}


void VulkanContext::RecordComputeCommandBuffer(VkCommandBuffer computeBuffer){
    BeginCommandBuffer(computeBuffer,0);
    vkCmdBindPipeline(computeBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline);
    vkCmdBindDescriptorSets(computeBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, &_computeDescriptorSets[currentFrame], 0, nullptr);
    vkCmdDispatch(computeBuffer, PARTICLE_COUNT / 64, 1, 1);
    VkBufferMemoryBarrier bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;   // compute 写
    bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT; // vertex 读
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = _computeBuffers[currentFrame]; // ⚠️ 这里要确保是 graphics 用来画的 buffer
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        computeBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,     // compute 之后
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,       // vertex 之前
        0,
        0, nullptr,
        1, &bufferBarrier,
        0, nullptr);

    EndCommandBuffer(computeBuffer);
}

void VulkanContext::CleanupCompute(){
    vkDestroyDescriptorSetLayout(_device, _computeDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(_device, _computeDescriptorPool, nullptr);
    vkDestroyPipeline(_device, _computePipeline, nullptr);
    vkDestroyPipelineLayout(_device, _computePipelineLayout, nullptr);
    for(auto& buffer: _computeBuffers){
        vkDestroyBuffer(_device, buffer, nullptr);
    }
    for(auto& commandBuffer: _computeCommandBuffers){
        vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
    }
    for(auto& fence: _computeFinishedFences){
        vkDestroyFence(_device, fence, nullptr);
    }
    for(auto& semaphore: _computeFinishedSemaphores){
        vkDestroySemaphore(_device, semaphore, nullptr);
    }
    for(auto& memory: _computeBuffersMemorys){
        vkFreeMemory(_device, memory, nullptr);
    }
}

void VulkanContext::CreateLogicDevice(){
    std::set<uint32_t> indices = {_queueFamily.graphicsQueueFamily.value(),_queueFamily.presentQueueFamily.value()};
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for(auto index:indices){
        VkDeviceQueueCreateInfo  queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = index;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
    VkDeviceCreateInfo logicInfo{};
    logicInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logicInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    logicInfo.pQueueCreateInfos = queueCreateInfos.data();
    logicInfo.pEnabledFeatures = &deviceFeatures;
    logicInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
    logicInfo.ppEnabledExtensionNames = DeviceExtensions.data();
    logicInfo.pNext = &dynamicRenderingFeatures;
    VkResult result = vkCreateDevice(_physicalDevice, &logicInfo, nullptr, &_device);
    if(result != VK_SUCCESS){
        throw std::runtime_error("Failed to create logic device");
    }
    vkGetDeviceQueue(_device, _queueFamily.graphicsQueueFamily.value(), 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, _queueFamily.graphicsQueueFamily.value(), 0, &_computeQueue); // 修复：从图形队列族获取计算队列
    vkGetDeviceQueue(_device, _queueFamily.presentQueueFamily.value(), 0, &_presentQueue);
    std::cout << "[SUCCESS] Logic device created" << std::endl;
    std::cout << "[SUCCESS] Graphics queue retrieved" << std::endl;
    std::cout << "[SUCCESS] Present queue retrieved" << std::endl;
}

void VulkanContext::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);
    if (vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }
    vkBindBufferMemory(_device, buffer, bufferMemory, 0);
}

void VulkanContext::CopyBuffer(VkBuffer src,VkBuffer dst,VkDeviceSize size){
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);
}

void VulkanContext::CreateVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(_vertices[0]) * _vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(_device, stagingBufferMemory);
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vertexBuffer, _vertexBufferMemory);
    CopyBuffer(stagingBuffer, _vertexBuffer, bufferSize);
    vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
    std::cout << "[SUCCESS] Vertex buffer created" << std::endl;
    std::cout << "VertexInfo size = " << sizeof(VertexInfo) << std::endl;
    std::cout << "Vertex count = " << _vertices.size() << std::endl;
    std::cout << "Vertex size = " << sizeof(_vertices[0]) << std::endl;
    std::cout << "Vertex stride = " << sizeof(VertexInfo) << std::endl;
}

void VulkanContext::CreateIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, _indices.data(), (size_t)bufferSize);
    vkUnmapMemory(_device, stagingBufferMemory);
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _indexBuffer, _indexBufferMemory);
    CopyBuffer(stagingBuffer, _indexBuffer, bufferSize);
    vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
    std::cout << "[SUCCESS] Index buffer created" << std::endl;
    std::cout << "IndexInfo size = " << sizeof(uint32_t) << std::endl;
    std::cout << "Index count = " << _indices.size() << std::endl;
}

void VulkanContext::CreateUniformBuffers(){
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    _uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    _uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    _uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    _uniformcomputeBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    _uniformcomputeBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    _uniformcomputeBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _uniformBuffers[i], _uniformBuffersMemory[i]);
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _uniformcomputeBuffers[i], _uniformcomputeBuffersMemory[i]);
    }
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        vkMapMemory(_device, _uniformBuffersMemory[i], 0, bufferSize, 0, &_uniformBuffersMapped[i]);
        vkMapMemory(_device, _uniformcomputeBuffersMemory[i], 0, bufferSize, 0, &_uniformcomputeBuffersMapped[i]);
    }
    std::cout<<"[SUCCESS] Uniform buffers created"<<std::endl;
    std::cout<<"UniformBufferObject size = "<<sizeof(UniformBufferObject)<<std::endl;
    std::cout<<"UniformBufferObject alignment = "<<alignof(UniformBufferObject)<<std::endl;
}

void VulkanContext::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout){
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (hasStencilComponent(format)) {
        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }
    vkCmdPipelineBarrier(
    commandBuffer,
    sourceStage, destinationStage,
    0,
    0, nullptr,
    0, nullptr,
    1, &barrier
    );
    EndSingleTimeCommands(commandBuffer);
}

void VulkanContext::TransitionImageLayout(VkImage image, VkFormat format, uint32_t mipLevels, VkImageLayout oldLayout, VkImageLayout newLayout){
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (hasStencilComponent(format)) {
        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL){
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if(oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR){
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;

        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }
    vkCmdPipelineBarrier(
    commandBuffer,
    sourceStage, destinationStage,
    0,
    0, nullptr,
    0, nullptr,
    1, &barrier
    );
    EndSingleTimeCommands(commandBuffer);
}

void VulkanContext::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height){
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };
    vkCmdCopyBufferToImage(
    commandBuffer,
    buffer,
    image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region
    );
    EndSingleTimeCommands(commandBuffer);
}

void VulkanContext::CreateDescriptorPool(){
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    if (vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
    std::cout<<"[SUCCESS] Descriptor pool created"<<std::endl;
}

void VulkanContext::CreateDescriptorSets(){
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, _descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();
    _descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(_device, &allocInfo, _descriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
    }
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = _uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = _textureImageView;
        imageInfo.sampler = _textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = _descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = _descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);  
    }
    std::cout<<"[SUCCESS] Descriptor sets created"<<std::endl;
}

void VulkanContext::CreateTextureImage(){
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    _mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(_device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, (size_t)imageSize);
    vkUnmapMemory(_device, stagingBufferMemory);
    stbi_image_free(pixels);
    CreateImage(texWidth, texHeight,_mipLevels,VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage, _textureImageMemory);
    TransitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_SRGB, _mipLevels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, _textureImage, (uint32_t)texWidth, (uint32_t)texHeight);
    GenerateMipmaps(_textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, _mipLevels);
    //TransitionImageLayout(_textureImage, VK_FORMAT_R8G8B8A8_SRGB, _mipLevels, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkDestroyBuffer(_device, stagingBuffer, nullptr);
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
}

void VulkanContext::CreateTextureImageView(){
    _textureImageView = CreateImageView(_textureImage, VK_FORMAT_R8G8B8A8_SRGB,_mipLevels, VK_IMAGE_ASPECT_COLOR_BIT);
    std::cout<<"[SUCCESS] Texture image view created"<<std::endl;
}

void VulkanContext::CreateTextureSampler(){
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(_physicalDevice, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f; // Optional
    //samplerInfo.minLod = static_cast<float>(_mipLevels / 2);
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.mipLodBias = 0.0f; // Optional
    if (vkCreateSampler(_device, &samplerInfo, nullptr, &_textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
    std::cout<<"[SUCCESS] Texture sampler created"<<std::endl;
}

void VulkanContext::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory){
        VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(_device, image, imageMemory, 0);
}

void VulkanContext::CreateImage(uint32_t width, uint32_t height,uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory){
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(_device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(_device, image, imageMemory, 0);
}

VkImageView VulkanContext::CreateImageView(VkImage image, VkFormat format,uint32_t mipLevels, VkImageAspectFlags aspectFlags){
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    VkImageView imageView;
    if (vkCreateImageView(_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
    return imageView;
}

VkImageView VulkanContext::CreateImageView(VkImage image, VkFormat format,VkImageAspectFlags aspectFlags){
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    VkImageView imageView;
    if (vkCreateImageView(_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
    return imageView;
}

void VulkanContext::GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels){
    if (mipLevels == 1) {
        return;
    }
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(_physicalDevice, imageFormat, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear sampling!");
    }
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;
    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkImageBlit blit{};
        blit.srcOffsets[0]=  { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        mipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        mipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
    }
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
    commandBuffer,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0,
    0, nullptr,
    0, nullptr,
    1, &barrier
    );
    EndSingleTimeCommands(commandBuffer);
}

void VulkanContext::CreateDepthResources(){
    VkFormat depthFormat = FindDepthFormat();
    CreateImage(_swapChainImageExtent.width, _swapChainImageExtent.height,1,_msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _depthImage, _depthImageMemory);
    _depthImageView = CreateImageView(_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);


    TransitionImageLayout(_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void VulkanContext::CreateColorResources(){
    VkFormat colorFormat = FindSupportedFormat({VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
    CreateImage(_swapChainImageExtent.width, _swapChainImageExtent.height, 1,_msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _colorImage, _colorImageMemory);
    _colorImageView = CreateImageView(_colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

VkSampleCountFlagBits VulkanContext::GetMaxUsableSampleCount(){
    //return VK_SAMPLE_COUNT_1_BIT;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(_physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { 
        std::cout << "VK_SAMPLE_COUNT_64_BIT" << std::endl;
        return VK_SAMPLE_COUNT_64_BIT; 
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { 
        std::cout << "VK_SAMPLE_COUNT_32_BIT" << std::endl;
        return VK_SAMPLE_COUNT_32_BIT; 
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { 
        std::cout << "VK_SAMPLE_COUNT_16_BIT" << std::endl;
        return VK_SAMPLE_COUNT_16_BIT; 
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { 
        std::cout << "VK_SAMPLE_COUNT_8_BIT" << std::endl;
        return VK_SAMPLE_COUNT_8_BIT; 
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { 
        std::cout << "VK_SAMPLE_COUNT_4_BIT" << std::endl;
        return VK_SAMPLE_COUNT_4_BIT; 
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { 
        std::cout << "VK_SAMPLE_COUNT_2_BIT" << std::endl;
        return VK_SAMPLE_COUNT_2_BIT; 
    }

    std::cout << "VK_SAMPLE_COUNT_1_BIT" << std::endl;
    return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanContext::UpdateUniformBuffer(uint32_t currentImage){
    auto now = std::chrono::high_resolution_clock::now();
    _deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(now - _lastFrameTime).count();
    _lastFrameTime = now;
    float time = _deltaTime;
    memcpy(_uniformcomputeBuffersMapped[currentImage], &time, sizeof(time));
    UniformBufferObject ubo;
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::mat4(1.0f);
    ubo.proj = glm::mat4(1.0f);
    memcpy(_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice device){
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    
    // Check if device supports graphics queue
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    if (queueFamilyCount == 0) {
        return false;
    }
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    // Find queue family that supports graphics operations
    bool hasGraphicsQueue = false;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            hasGraphicsQueue = true;
            break;
        }
    }
    
    if (!hasGraphicsQueue) {
        return false;
    }
    
    // Check if device supports required extensions
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    if(extensionCount == 0){
        return false;
    }
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    
    // Check if swapchain extension is supported
    bool hasSwapChainSupport = false;
    for (const auto& extension : availableExtensions) {
        if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapChainSupport = true;
            break;
        }
    }
    
    if (!hasSwapChainSupport) {
        return false;
    }
    
    // Check swapchain support details
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &capabilities);
    
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);
    
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
    
    if (formatCount == 0 || presentModeCount == 0) {
        return false;
    }
    
         // Check device type preference (prefer discrete GPU)
     if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
         std::cout << "[INFO] Selected discrete GPU: " << deviceProperties.deviceName << std::endl;
     } else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
         std::cout << "[INFO] Selected integrated GPU: " << deviceProperties.deviceName << std::endl;
     } else {
         std::cout << "[INFO] Selected other device: " << deviceProperties.deviceName << std::endl;
     }
    
    _queueFamily = FindQueueFamilyIndex(device);
    if(_queueFamily.isComplete()){
        std::cout << "[SUCCESS] Queue families found" << std::endl;
    }

    bool extensionSupport = CheckExtensionSupport(device);
    if(extensionSupport){
        std::cout << "[SUCCESS] Device extensions supported" << std::endl;
    }
    
    bool swapChainSupport = false;
    if(extensionSupport){
        swapChainSupport = GetSwapChainSupportDetails(device).formats.size() > 0 && GetSwapChainSupportDetails(device).presentModes.size() > 0;
        if(swapChainSupport){
            std::cout << "[SUCCESS] Swap chain support verified" << std::endl;
        }
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return _queueFamily.isComplete()&&extensionSupport&&swapChainSupport&&supportedFeatures.samplerAnisotropy;
}

QueueFamily VulkanContext::FindQueueFamilyIndex(VkPhysicalDevice device){
    QueueFamily queueFamily;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    for(uint32_t i=0;i<queueFamilyCount;i++){
        if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT&&queueFamilies[i].queueFlags&VK_QUEUE_COMPUTE_BIT){
            queueFamily.graphicsQueueFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
        if(presentSupport){
            queueFamily.presentQueueFamily = i;
        }
        if(queueFamily.isComplete()){
            break;
        }
    }
    return queueFamily;
}

void VulkanContext::CleanupAll(){
    CleanupVulkan();
    CleanupWindow();
}

void VulkanContext::CleanupWindow(){
    glfwDestroyWindow(_window);
    glfwTerminate();
}

void VulkanContext::CleanupVulkan(){
    CleanupSwapChain();
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
        vkDestroyBuffer(_device, _uniformBuffers[i], nullptr);
        vkFreeMemory(_device, _uniformBuffersMemory[i], nullptr);
        vkDestroyBuffer(_device, _uniformcomputeBuffers[i], nullptr);
        vkFreeMemory(_device, _uniformcomputeBuffersMemory[i], nullptr);
    }
    CleanupCompute();
    vkDestroyImageView(_device, _textureImageView, nullptr);
    vkDestroySampler(_device, _textureSampler, nullptr);
    vkDestroyImage(_device, _textureImage, nullptr);
    vkFreeMemory(_device, _textureImageMemory, nullptr);
    vkDestroyBuffer(_device, _indexBuffer, nullptr);
    vkFreeMemory(_device, _indexBufferMemory, nullptr);
    vkDestroyBuffer(_device, _vertexBuffer, nullptr);
    vkFreeMemory(_device, _vertexBufferMemory, nullptr);
    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    for(int i=0;i<MAX_FRAMES_IN_FLIGHT;i++){
    vkDestroySemaphore(_device, _imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(_device, _renderFinishedSemaphores[i], nullptr);
    vkDestroyFence(_device, _inFlightFences[i], nullptr);
    }
    vkDestroyCommandPool(_device, _commandPool, nullptr);
    vkDeviceWaitIdle(_device);
    vkDestroyPipeline(_device,_pipeline,nullptr);
    vkDestroyPipelineLayout(_device,_pipelineLayout,nullptr);
    vkDestroyDescriptorSetLayout(_device,_descriptorSetLayout,nullptr);
    DestroyDebugMessenger();
    vkDestroyDevice(_device, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyInstance(_instance, nullptr);
}

void VulkanContext::CleanupSwapChain(){
    vkDestroyImageView(_device, _colorImageView, nullptr);
    vkDestroyImage(_device, _colorImage, nullptr);
    vkFreeMemory(_device, _colorImageMemory, nullptr);
    vkDestroyImageView(_device, _depthImageView, nullptr);
    vkDestroyImage(_device, _depthImage, nullptr);
    vkFreeMemory(_device, _depthImageMemory, nullptr);
    for(auto& iv : _imageViews){
        vkDestroyImageView(_device,iv,nullptr);
    }
    vkDestroySwapchainKHR(_device, _swapChain , nullptr);
}

void VulkanContext::RecreateSwapChain(){
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(_device);
    CleanupSwapChain();
    CreateSwapChain();
    GetSwapChainImages(_swapChainImages);
    CreateColorResources();
    CreateImageViews();
    CreateDepthResources();
}

GLFWwindow* VulkanContext::GetWindow(){
    return _window;
}

SwapChainSupportDetails VulkanContext::GetSwapChainSupportDetails(VkPhysicalDevice device){
    SwapChainSupportDetails details;
    details.capabilities = GetSwapChainCapabilities(device);
    details.formats = GetSwapChainFormat(device);
    details.presentModes = GetSwapChainPresentMode(device);
    return details;
}

VkSurfaceCapabilitiesKHR VulkanContext::GetSwapChainCapabilities(VkPhysicalDevice device){
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &capabilities);
    return capabilities;
}

std::vector<VkSurfaceFormatKHR> VulkanContext::GetSwapChainFormat(VkPhysicalDevice device){
    std::vector<VkSurfaceFormatKHR> formats;
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);
    if(formatCount != 0){
        formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, formats.data());
    }
    return formats;
}

std::vector<VkPresentModeKHR> VulkanContext::GetSwapChainPresentMode(VkPhysicalDevice device){
    std::vector<VkPresentModeKHR> presentModes;
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
    if(presentModeCount != 0){
        presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, presentModes.data());
    }
    return presentModes;
}

VkSurfaceFormatKHR VulkanContext::ChooseSurfaceFMT(const SwapChainSupportDetails& swapChain){
    const std::vector<VkSurfaceFormatKHR>& formats = swapChain.formats;
    if(formats.empty()){
        throw std::runtime_error("No surface formats available!");
    }
    for(auto& f : formats){
        if(f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
            return f;
        }
    }
    return formats[0];
}

VkPresentModeKHR VulkanContext::ChoosePresentMode(const SwapChainSupportDetails& swapchain){
    const std::vector<VkPresentModeKHR>& presentModes = swapchain.presentModes;
    for(auto& p : presentModes){
        if(p == VK_PRESENT_MODE_MAILBOX_KHR){
            return p;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::ChooseExtent2D(const SwapChainSupportDetails& swapchain){
    const VkSurfaceCapabilitiesKHR capabilities = swapchain.capabilities;
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()){
        return capabilities.currentExtent;
    }
    else{
        int w,h;
        glfwGetFramebufferSize(_window, &w,&h);
        VkExtent2D actualExtent = {static_cast<uint32_t>(w),static_cast<uint32_t>(h)};

        actualExtent.width = std::clamp(actualExtent.width,capabilities.minImageExtent.width,capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,capabilities.minImageExtent.height,capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

std::vector<char> VulkanContext::ReadFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open " + filePath);
    }

    std::cout << "Open " << filePath << std::endl;

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> fileData(fileSize);

    file.seekg(0);
    file.read(fileData.data(), fileSize);

    file.close();
    return fileData;
}

VkShaderModule VulkanContext::CreateShaderModule(const std::vector<char>& code){
    VkShaderModuleCreateInfo SMCreateInfo{};
    SMCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    SMCreateInfo.codeSize = code.size();
    SMCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;
    VkResult result;
    result = vkCreateShaderModule(_device,&SMCreateInfo,nullptr,&shaderModule);
    if (result != VK_SUCCESS) {
        switch (result) {
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                throw std::runtime_error("Failed to create shader module: Out of host memory!");
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                throw std::runtime_error("Failed to create shader module: Out of device memory!");
            case VK_ERROR_INVALID_SHADER_NV:
                throw std::runtime_error("Failed to create shader module: Invalid shader!");
            default:
                throw std::runtime_error("Failed to create shader module: Unknown error (" + std::to_string(result) + ")");
        }
    }
    return shaderModule;
}

VkCommandBuffer VulkanContext::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanContext::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_graphicsQueue);

    vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
}

void VulkanContext::BeginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags){
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = nullptr;
    if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin command buffer!");
    }
}

void VulkanContext::EndCommandBuffer(VkCommandBuffer commandBuffer){
    if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS){
        throw std::runtime_error("Failed to end command buffer!");
    }
}

void VulkanContext::RecordCommandBuffer(VkCommandBuffer commandBuffer,uint32_t imageIndex){
    BeginCommandBuffer(commandBuffer,0);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.clearValue = clearValues[0];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.imageView = _colorImageView;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.resolveImageView = _imageViews[imageIndex];
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.clearValue = clearValues[1];
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.imageView = _depthImageView;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset.x = 0;
    renderingInfo.renderArea.offset.y = 0;
    renderingInfo.renderArea.extent = _swapChainImageExtent;
    renderingInfo.layerCount = 1;
    renderingInfo.viewMask = 0;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_swapChainImageExtent.width);
    viewport.height = static_cast<float>(_swapChainImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = _swapChainImageExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

    VkBuffer vertexBuffers[] = {_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSets[currentFrame], 0, nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &_computeBuffers[currentFrame], offsets);
    vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);
    vkCmdEndRendering(commandBuffer);
    EndCommandBuffer(commandBuffer);
}

uint32_t VulkanContext::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

VkFormat VulkanContext::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features){
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("Failed to find supported format!");
}

VkFormat VulkanContext::FindDepthFormat(){
    return FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void VulkanContext::DrawFrame(){
    vkWaitForFences(_device, 1, &_inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapChain, std::numeric_limits<uint64_t>::max(), _imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if(result==VK_ERROR_OUT_OF_DATE_KHR||framebufferResized){
        framebufferResized = false;
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    vkResetFences(_device, 1, &_inFlightFences[currentFrame]);

    vkResetCommandBuffer(_commandBuffers[currentFrame], 0);
    TransitionImageLayout(_swapChainImages[imageIndex],_swapChainImageFMT,1,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    RecordCommandBuffer(_commandBuffers[currentFrame], imageIndex);
    TransitionImageLayout(_swapChainImages[imageIndex],_swapChainImageFMT,1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    UpdateUniformBuffer(imageIndex);
    VkSemaphore waitSemaphores[] = {_imageAvailableSemaphores[currentFrame]};
    VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[currentFrame]};
    VkPipelineStageFlags waitPipelineStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitPipelineStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffers[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    if(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[currentFrame]) != VK_SUCCESS){
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkSwapchainKHR swapChains[] = {_swapChain};
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.swapchainCount = 1;
    presentInfo.pResults = nullptr;
    VkResult presentResult = vkQueuePresentKHR(_presentQueue, &presentInfo);
    if(presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR){
        RecreateSwapChain();
    }
    else if(presentResult != VK_SUCCESS){
        throw std::runtime_error("Failed to present swap chain image!");
    }
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::DrawCompute(){
    // Compute submission
    vkWaitForFences(_device, 1, &_computeFinishedFences[currentFrame], VK_TRUE, UINT64_MAX);
    
    UpdateUniformBuffer(currentFrame);
    UpdateComputeDescriptors(currentFrame);

    vkResetFences(_device, 1, &_computeFinishedFences[currentFrame]);

    vkResetCommandBuffer(_computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    RecordComputeCommandBuffer(_computeCommandBuffers[currentFrame]);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_computeCommandBuffers[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_computeFinishedSemaphores[currentFrame];

    if (vkQueueSubmit(_computeQueue, 1, &submitInfo, _computeFinishedFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit compute command buffer!");
    };

    // Graphics submission
    vkWaitForFences(_device, 1, &_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);


    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapChain, std::numeric_limits<uint64_t>::max(), _imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if(result==VK_ERROR_OUT_OF_DATE_KHR||framebufferResized){
        framebufferResized = false;
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(_device, 1, &_inFlightFences[currentFrame]);
    vkResetCommandBuffer(_commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    TransitionImageLayout(_swapChainImages[imageIndex],_swapChainImageFMT,1,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    RecordCommandBuffer(_commandBuffers[currentFrame], imageIndex);
    TransitionImageLayout(_swapChainImages[imageIndex],_swapChainImageFMT,1,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[currentFrame]};
    VkSemaphore waitSemaphores[] = { _computeFinishedSemaphores[currentFrame], _imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.waitSemaphoreCount = 2;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffers[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_renderFinishedSemaphores[currentFrame];

    if (vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkSwapchainKHR swapChains[] = {_swapChain};
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.swapchainCount = 1;
    presentInfo.pResults = nullptr;
    VkResult presentResult = vkQueuePresentKHR(_presentQueue, &presentInfo);
    if(presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR){
        RecreateSwapChain();
    }
    else if(presentResult != VK_SUCCESS){
        throw std::runtime_error("Failed to present swap chain image!");
    }
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::Run(){
    std::cout<<"[INFO] Running"<<std::endl;
    long long frameCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        DrawCompute();
        frameCount++;
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    std::cout << "FPS: " << (double)frameCount/(double)duration.count() << std::endl;
    vkDeviceWaitIdle(_device);
}