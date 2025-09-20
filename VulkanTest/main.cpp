#include "Dependencies.h"
#include <vulkan/vulkan.h>
#include <chrono>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include <glm/gtx/string_cast.hpp>

struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // ŷ���� (��)
    glm::vec3 scale = glm::vec3(1.0f);
};

struct LightParams {
    glm::vec3 direction = glm::vec3(-0.5f, -1.0f, -0.8f);
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float     intensity = 10.0f;
};

struct SkyUniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
};

void updateUniformBuffer(
    VulkanBuffer& uniformBuffer,
    VulkanBuffer& skyUniformBuffer,
    VkExtent2D extent,
    const Transform& modelTransform,
    const Transform& cameraTransform,
    const LightParams& lightParams
) {
    // --- 1. ����ԭʼ�ġ�δת�õľ��� ---

    // Model Matrix
    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat = glm::translate(modelMat, modelTransform.position);
    modelMat = glm::rotate(modelMat, glm::radians(modelTransform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    modelMat = glm::rotate(modelMat, glm::radians(modelTransform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    modelMat = glm::rotate(modelMat, glm::radians(modelTransform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    modelMat = glm::scale(modelMat, modelTransform.scale);

    // View Matrix
    glm::mat4 camWorldMat = glm::mat4(1.0f);
    camWorldMat = glm::translate(camWorldMat, cameraTransform.position);
    camWorldMat = glm::rotate(camWorldMat, glm::radians(cameraTransform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    camWorldMat = glm::rotate(camWorldMat, glm::radians(cameraTransform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    camWorldMat = glm::rotate(camWorldMat, glm::radians(cameraTransform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 viewMat = glm::inverse(camWorldMat); // <-- ����ԭʼ�ġ���ȷ����ͼ����

    // Projection Matrix
    glm::mat4 projMat = glm::perspective(glm::radians(45.0f), (float)extent.width / (float)extent.height, 0.1f, 100.0f);
    projMat[1][1] *= -1;

    // --- 2. ׼����д���� UBO (Ϊ HLSL ת��) ---
    UniformBufferObject ubo{};
    ubo.model = glm::transpose(modelMat);
    ubo.view = glm::transpose(viewMat);
    ubo.proj = glm::transpose(projMat);
    ubo.camPos = glm::vec4(cameraTransform.position, 1.0f);
    ubo.lightDir = glm::vec4(glm::normalize(lightParams.direction), 0.0f);
    ubo.lightColor = glm::vec4(lightParams.color, lightParams.intensity);
    uniformBuffer.setData(&ubo, sizeof(ubo));

    // --- 3. ׼����д����պ� UBO ---
    SkyUniformBufferObject subo{};
    // vvvvvvvvvvvvvvvv �ؼ��޸� vvvvvvvvvvvvvvvv
    // ��ԭʼ�ġ�δת�õ� viewMat ���Ƴ�ƽ��
    glm::mat4 skyViewMat = glm::mat4(glm::mat3(viewMat));
    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    subo.view = glm::transpose(skyViewMat);
    subo.proj = glm::transpose(projMat);
    skyUniformBuffer.setData(&subo, sizeof(subo));
}

int main()
{
    Transform modelTransform;
    Transform cameraTransform;
    cameraTransform.position = glm::vec3(0.0f, 1.0f, 5.0f); // ��ʼλ�ã���Z��������5����λ��Y���1����λ
    LightParams lightParams;
    WindowInfo info(1280, 800,"Vulkan");

    VulkanContext context(info);

    VulkanQueue graphicsQueue(context, QueueType::Graphics);

    VulkanQueue presentQueue(context, QueueType::Present);

    Renderer renderer(context,3);

    ImmediateSubmitter immediateSubmitter(context, graphicsQueue);

    VulkanSwapchain swapchain(context);

    Model model("res\\model.obj");
	std::unique_ptr<VulkanImage> diffuseImage = VulkanImage::createTexture(context, immediateSubmitter, "res\\diffuse.png", true);
    std::unique_ptr<VulkanImage> normalImage = VulkanImage::createTexture(context, immediateSubmitter, "res\\normal.png",false,VK_FORMAT_R8G8B8A8_UNORM);
    std::unique_ptr<VulkanImage> emissiveImage = VulkanImage::createTexture(context, immediateSubmitter, "res\\emissive.png",false);
    std::unique_ptr<VulkanImage> msaImage = VulkanImage::createTexture(context, immediateSubmitter, "res\\msa.png", false);

    std::map<std::string, std::unique_ptr<VulkanImage>> skyboxCubemaps;
    skyboxCubemaps["Purple Sky"] = VulkanImage::createCubemap(context, immediateSubmitter, "res\\PurpleSky");
    skyboxCubemaps["Blue Sky"] = VulkanImage::createCubemap(context, immediateSubmitter, "res\\BuleSky");
    skyboxCubemaps["Building Sky"] = VulkanImage::createCubemap(context, immediateSubmitter, "res\\BuildSky");
    skyboxCubemaps["Red Sky"] = VulkanImage::createCubemap(context, immediateSubmitter, "res\\RedSky");
    VulkanImage* currentSkybox = skyboxCubemaps.begin()->second.get();

    std::cout << "All SkyBoxes Built!" << std::endl;

	std::cout << "SkyBox Build!"<< std::endl;

    VulkanBuffer vertexBuffer(
        context, model.getVertices().size() * sizeof(Vertex), 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VulkanBuffer indexBuffer(context, model.getIndices().size() * sizeof(uint32_t), 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    immediateSubmitter.copyDataToBuffer(model.getVertices().data(), vertexBuffer, model.getVertices().size() * sizeof(Vertex));
    immediateSubmitter.copyDataToBuffer(model.getIndices().data(), indexBuffer, model.getIndices().size() * sizeof(uint32_t));

    VulkanBuffer skyboxVertexBuffer(
        context, skyboxVertices.size() * sizeof(SkyboxVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    VulkanBuffer skyboxIndexBuffer(
        context, skyboxIndices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    immediateSubmitter.copyDataToBuffer((void*)skyboxVertices.data(), skyboxVertexBuffer, skyboxVertices.size() * sizeof(SkyboxVertex));
    immediateSubmitter.copyDataToBuffer((void*)skyboxIndices.data(), skyboxIndexBuffer, skyboxIndices.size() * sizeof(uint32_t));

    std::vector<std::unique_ptr<VulkanBuffer>> uniformBuffers;
    std::vector<std::unique_ptr<VulkanBuffer>> skyUniformBuffers;
    for (int i = 0; i < renderer.getMaxFramesInFlight(); ++i) {
        uniformBuffers.push_back(
            std::make_unique<VulkanBuffer>(
                context, sizeof(UniformBufferObject),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            )
        );        
        skyUniformBuffers.push_back(
            std::make_unique<VulkanBuffer>(
                context, sizeof(SkyUniformBufferObject),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            )
        );
    }

    PipelineBuilder pipelineBuilder(context);
    pipelineBuilder.addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "res\\vert.spv","VSMain");
    pipelineBuilder.addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "res\\frag.spv","PSMain");
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

    VulkanDescriptorSetLayout::Builder descriptorSetLayoutBuilder(context);
    descriptorSetLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT| VK_SHADER_STAGE_FRAGMENT_BIT);
	descriptorSetLayoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptorSetLayoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptorSetLayoutBuilder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    descriptorSetLayoutBuilder.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

    std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout = descriptorSetLayoutBuilder.build();

	VulkanDescriptorPool::Builder descriptorPoolBuilder(context);
	descriptorPoolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, renderer.getMaxFramesInFlight());
	descriptorPoolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4);
	descriptorPoolBuilder.setMaxSets(renderer.getMaxFramesInFlight());
	descriptorPoolBuilder.setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
	std::unique_ptr<VulkanDescriptorPool> descriptorPool = descriptorPoolBuilder.build();

    std::vector<VkDescriptorSet> descriptorSets(renderer.getMaxFramesInFlight());
    for (int i = 0; i < renderer.getMaxFramesInFlight(); ++i) {
        descriptorSets[i] = descriptorPool->allocateSet(*descriptorSetLayout);

        VkDescriptorBufferInfo bufferInfo = uniformBuffers[i]->getDescriptorInfo();


		VkDescriptorImageInfo imageInfo1 = diffuseImage->getDescriptorInfo();
		VkDescriptorImageInfo imageInfo2 = normalImage->getDescriptorInfo();
		VkDescriptorImageInfo imageInfo3 = emissiveImage->getDescriptorInfo();
		VkDescriptorImageInfo imageInfo4 = msaImage->getDescriptorInfo();
        DescriptorWriter(context, descriptorSets[i])
            .writeBuffer(0, &bufferInfo)
			.writeImage(1, &imageInfo1)
			.writeImage(2, &imageInfo2)
			.writeImage(3, &imageInfo3)
			.writeImage(4, &imageInfo4)
            .update();
    }

    pipelineBuilder.addDescriptorSetLayout(descriptorSetLayout->getLayout());

    std::unique_ptr<VulkanPipeline> graphicsPipeline = pipelineBuilder.buildGraphicsPipeline();

    PipelineBuilder skyPipelineBuilder(context);
    skyPipelineBuilder.addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, "res\\skyvert.spv", "VSMain");
    skyPipelineBuilder.addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, "res\\skyfrag.spv", "PSMain");
    skyPipelineBuilder.setVertexInputState({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = SkyboxVertex::getBindingDescriptions().data(),
        .vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = SkyboxVertex::getAttributeDescriptions().data(),
        });
    skyPipelineBuilder.setInputAssemblyState({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
        });
    skyPipelineBuilder.setRenderingFormats(swapchain.getImageFormat(), context.findDepthFormat());

    VkPipelineDepthStencilStateCreateInfo skyDepthStencil{};
    skyDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    skyDepthStencil.depthTestEnable = VK_TRUE;
    skyDepthStencil.depthWriteEnable = VK_FALSE; // <-- ��д�����
    skyDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // <-- ��������Զ������
    skyDepthStencil.stencilTestEnable = VK_FALSE;
    skyPipelineBuilder.setDepthStencilState(skyDepthStencil);

    // --- Ϊ��պ���������Ĺ�դ��״̬ ---
    VkPipelineRasterizationStateCreateInfo skyRasterizer{};
    skyRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    skyRasterizer.depthClampEnable = VK_FALSE;
    skyRasterizer.rasterizerDiscardEnable = VK_FALSE;
    skyRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    skyRasterizer.lineWidth = 1.0f;
    skyRasterizer.cullMode = VK_CULL_MODE_FRONT_BIT; // <-- �޳�����
    skyRasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // ������ģ��һ��
    skyRasterizer.depthBiasEnable = VK_FALSE;
    skyPipelineBuilder.setRasterizationState(skyRasterizer);

    VulkanDescriptorSetLayout::Builder skyDescriptorSetLayoutBuilder(context);
    skyDescriptorSetLayoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    skyDescriptorSetLayoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    std::unique_ptr<VulkanDescriptorSetLayout> skyDescriptorSetLayout = skyDescriptorSetLayoutBuilder.build();

    VulkanDescriptorPool::Builder skyDescriptorPoolBuilder(context);
    skyDescriptorPoolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, renderer.getMaxFramesInFlight());
	skyDescriptorPoolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, renderer.getMaxFramesInFlight());
    skyDescriptorPoolBuilder.setMaxSets(renderer.getMaxFramesInFlight());
    skyDescriptorPoolBuilder.setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
    std::unique_ptr<VulkanDescriptorPool> skyDescriptorPool = skyDescriptorPoolBuilder.build();

    std::vector<VkDescriptorSet> skyDescriptorSets(renderer.getMaxFramesInFlight());
    for (int i = 0; i < renderer.getMaxFramesInFlight(); ++i) {
        skyDescriptorSets[i] = skyDescriptorPool->allocateSet(*skyDescriptorSetLayout);
    }

    skyPipelineBuilder.addDescriptorSetLayout(skyDescriptorSetLayout->getLayout());

    std::unique_ptr<VulkanPipeline> skyGraphicsPipeline = skyPipelineBuilder.buildGraphicsPipeline();

#pragma region ImGuiSetting


    VkDescriptorPoolSize pool_sizes[] = {
       { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
       { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
       { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
       { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
       { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
       { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
       { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
       { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
       { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
       { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
       { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    if (vkCreateDescriptorPool(context.getDevice(), &pool_info, nullptr, &imguiPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    // 2. ��ʼ�� ImGui ������
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // ���ü��̿���
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // ����ͣ��
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // ���ö��ӿ�

    // 3. ���� ImGui ���
    ImGui::StyleColorsDark();

    // 4. ��ʼ�� ImGui ���
    ImGui_ImplGlfw_InitForVulkan(context.getWindow(), true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = context.getInstance();
    init_info.PhysicalDevice = context.getPhysicalDevice();
    init_info.Device = context.getDevice();
    init_info.Queue = graphicsQueue.getQueue(); // ImGui ����Ⱦ�����ͼ�ζ�����ִ��
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3; // ��������Ľ�����ͼ������ƥ��
    init_info.ImageCount = 3;    // ��������Ľ�����ͼ������ƥ��
    init_info.MSAASamples = context.getMsaaSamples(); // ���� ImGui ��ʹ�õ� MSAA �ȼ�

    // 5. ���� ImGui ��Ⱦ��Ŀ��
    // ע�⣺ImGui ��Ҫ֪����̬��Ⱦ����ɫ������ʽ
    VkPipelineRenderingCreateInfo rendering_info = {};
    rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    VkFormat colorFormat = swapchain.getImageFormat();
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &colorFormat;
    // ������� ImGui ��Ⱦͨ����Ҳʹ����ȣ�����Ҫ����
    // VkFormat depthFormat = _context->findDepthFormat();
    // rendering_info.depthAttachmentFormat = depthFormat;

    init_info.PipelineRenderingCreateInfo = rendering_info;
	init_info.UseDynamicRendering = true; // ���ö�̬��Ⱦ


    // ���þɰ汾�� Init ����
    ImGui_ImplVulkan_Init(&init_info);

#pragma endregion

    while (!glfwWindowShouldClose(context.getWindow())) {
        glfwPollEvents();
        context.CheckFormatChange(swapchain);

#pragma region ImGuiRender


        // 1. ��ʼ�µ�һ֡ (ImGui)
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. �������� ImGui ����
        ImGui::Begin("My Controls");
        ImGui::Text("Frame Time: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        if (ImGui::CollapsingHeader("Model Transform")) {
            ImGui::DragFloat3("Position##Model", &modelTransform.position.x, 0.01f);
            ImGui::DragFloat3("Rotation##Model", &modelTransform.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale##Model", &modelTransform.scale.x, 0.01f);
        }

        // -- Camera Transform --
        if (ImGui::CollapsingHeader("Camera Transform")) {
            // DragFloat3 ���ڿ���ֱ�ӿ��������λ�ú���ת�Ƕ�
            ImGui::DragFloat3("Position##Camera", &cameraTransform.position.x, 0.1f);
            ImGui::DragFloat3("Rotation##Camera", &cameraTransform.rotation.x, 1.0f);
        }

        // -- Light Properties --
        if (ImGui::CollapsingHeader("Light Properties")) {
            ImGui::ColorEdit3("Color##Light", &lightParams.color.r);
            ImGui::DragFloat("Intensity##Light", &lightParams.intensity, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat3("Direction##Light", &lightParams.direction.x, 0.01f, -1.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Skybox Selection")) {
            // �������� map �е�������պ�
            for (auto const& [name, skybox_ptr] : skyboxCubemaps) {
                // ImGui::RadioButton �ᴴ��һ����ѡ��ť
                // �����ǰѡ����������պУ���ť�ᱻѡ��
                if (ImGui::RadioButton(name.c_str(), currentSkybox == skybox_ptr.get())) {
                    // ����û�����������ť���͸��µ�ǰѡ��
                    currentSkybox = skybox_ptr.get();
                }
            }
        }
        ImGui::End();

        // 3. ׼�� ImGui �Ļ�ͼ����
        ImGui::Render();
#pragma endregion


		uint32_t size = static_cast<uint32_t>(model.getIndices().size());
        VkCommandBuffer commandBuffer = renderer.beginFrame(swapchain);
        updateUniformBuffer(
            *uniformBuffers[renderer.getCurrentFrame()],
			*skyUniformBuffers[renderer.getCurrentFrame()],
            swapchain.getExtent(),
            modelTransform,
            cameraTransform,
            lightParams
        );

        VkDescriptorBufferInfo skyBufferInfo = skyUniformBuffers[renderer.getCurrentFrame()]->getDescriptorInfo();
        VkDescriptorImageInfo skyImageInfo = currentSkybox->getDescriptorInfo(); // <-- ʹ�õ�ǰѡ�����պ�

        DescriptorWriter(context, skyDescriptorSets[renderer.getCurrentFrame()])
            .writeBuffer(0, &skyBufferInfo)
            .writeImage(1, &skyImageInfo)
            .update();

        renderer.beginRenderPass(commandBuffer, swapchain);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipeline());

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchain.getExtent().width);
        viewport.height = static_cast<float>(swapchain.getExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = swapchain.getExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindIndexBuffer(commandBuffer, indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipeline->getLayout(), // ʹ�ù����Լ��Ĳ���
            0,                             // �󶨵� set 0
            1,                             // �� 1 �� set
            &descriptorSets[renderer.getCurrentFrame()], // ���뵱ǰ֡�� descriptor set
            0,
            nullptr
        );

        VkBuffer vertexBuffers[] = { vertexBuffer.getBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdDrawIndexed(commandBuffer, size, 1, 0, 0, 0);

		// ������պ�
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyGraphicsPipeline->getPipeline());
        vkCmdBindIndexBuffer(commandBuffer, skyboxIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            skyGraphicsPipeline->getLayout(), // ʹ�ù����Լ��Ĳ���
            0,                             // �󶨵� set 0
            1,                             // �� 1 �� set
            &skyDescriptorSets[renderer.getCurrentFrame()], // ���뵱ǰ֡�� descriptor set
            0,
            nullptr
        );

        VkBuffer skyVertexBuffers[] = { skyboxVertexBuffer.getBuffer() };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, skyVertexBuffers, offsets);

        vkCmdDrawIndexed(commandBuffer, skyboxIndices.size(), 1, 0, 0, 0);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
        renderer.endRenderPass(commandBuffer, swapchain);
        renderer.endFrame(swapchain, graphicsQueue, presentQueue);
    }
#pragma region destory


	vkDeviceWaitIdle(context.getDevice()); 
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(context.getDevice(), imguiPool, nullptr);

#pragma endregion
}