#include "Dependencies.h"
#include <vulkan/vulkan.h>

int main()
{
    WindowInfo info(800, 600,"Vulkan");
    VulkanContext context(info);
    VulkanQueue graphicsQueue(context, QueueType::Graphics);
    VulkanQueue presentQueue(context, QueueType::Present);
    Renderer renderer(context,3);
    ImmediateSubmitter immediateSubmitter(context, graphicsQueue);
    VulkanSwapchain swapchain(context);
    Model model("res\\model.obj");
    VulkanBuffer vertexBuffer(context, model.getVertices().size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VulkanBuffer indexBuffer(context, model.getIndices().size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    immediateSubmitter.copyDataToBuffer(model.getVertices().data(), vertexBuffer, model.getVertices().size() * sizeof(Vertex));
    immediateSubmitter.copyDataToBuffer(model.getIndices().data(), indexBuffer, model.getIndices().size() * sizeof(uint32_t));
    PipelineBuilder pipelineBuilder(context);
    pipelineBuilder.addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "res\\vert.spv","VSMain");
    pipelineBuilder.addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "res\\frag.spv","FSMain");
    pipelineBuilder.setVertexInputState({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = (uint32_t)model.getVertexBindingDescription().size(),
        .pVertexBindingDescriptions = model.getVertexBindingDescription().data(),
        .vertexAttributeDescriptionCount = (uint32_t)model.getVertexAttributeDescription().size(),
        .pVertexAttributeDescriptions = model.getVertexAttributeDescription().data(),
    });
    pipelineBuilder.setInputAssemblyState({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    });
    pipelineBuilder.setRenderingFormats(swapchain.getImageFormat(), context.findDepthFormat());
    //VulkanDescriptorSetLayout::Builder descriptorSetLayoutBuilder(context);
    //descriptorSetLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
    //descriptorSetLayoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    //std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout = descriptorSetLayoutBuilder.build();
}
