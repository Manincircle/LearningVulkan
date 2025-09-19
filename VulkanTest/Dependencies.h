#pragma once
#include <vulkan/vulkan.h>
#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanPipeline.h"
#include "ImmediateSubmitter.h"
#include "Renderer.h"
#include "VulkanQueue.h"
#include "Model.h"
#include "DescriptorWriter.h"
#include "VulkanDescriptorSetLayout.h"
#include "VulkanDescriptorPool.h"

struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 lightDir;   // ʹ�� vec4 ��ȷ������, w ����δʹ��
        glm::vec4 lightColor; // ʹ�� vec4 ��ȷ������, w �����ǹ��ǿ��
        glm::vec4 camPos;     // ʹ�� vec4 ��ȷ������, w ����δʹ��
};