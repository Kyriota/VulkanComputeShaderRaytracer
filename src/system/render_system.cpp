#include "render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>
#include <string>

namespace lve
{

    struct SimplePushConstantData
    {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    void renderGameObjects(FrameInfo &frameInfo, VkPipelineLayout graphicPipelineLayout, LveGraphicPipeline *graphicPipeline)
    {
        bind(frameInfo.commandBuffer, graphicPipeline->getPipeline());

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicPipelineLayout,
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr);

        for (auto &kv : frameInfo.gameObjects)
        {
            auto &obj = kv.second;
            if (obj.model == nullptr)
                continue;
            SimplePushConstantData push{};
            push.modelMatrix = obj.transform.mat4();
            push.normalMatrix = obj.transform.normalMatrix();

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                graphicPipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(SimplePushConstantData),
                &push);
            obj.model->bind(frameInfo.commandBuffer);
            obj.model->draw(frameInfo.commandBuffer);
        }
    }

    void renderScreenTexture(FrameInfo &frameInfo, VkPipelineLayout graphicPipelineLayout, LveGraphicPipeline *graphicPipeline)
    {
        bind(frameInfo.commandBuffer, graphicPipeline->getPipeline());

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicPipelineLayout,
            0,
            1,
            &frameInfo.textureSampleDescriptorSet,
            0,
            nullptr);

        vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
    }

    RenderSystem::RenderSystem(
        LveDevice &device,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout,
        GraphicPipelineConfigInfo &graphicPipelineConfigInfo)
        : lveDevice{device}
    {
        createGraphicPipelineLayout(globalSetLayout);
        createGraphicPipeline(renderPass, graphicPipelineConfigInfo);
    }

    RenderSystem::~RenderSystem()
    {
        vkDestroyPipelineLayout(lveDevice.device(), graphicPipelineLayout, nullptr);
    }

    void RenderSystem::createGraphicPipelineLayout(VkDescriptorSetLayout globalSetLayout)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SimplePushConstantData);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &graphicPipelineLayout) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void RenderSystem::createGraphicPipeline(VkRenderPass renderPass, GraphicPipelineConfigInfo &graphicPipelineConfigInfo)
    {
        assert(graphicPipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

        graphicPipelineConfigInfo.renderPass = renderPass;
        graphicPipelineConfigInfo.pipelineLayout = graphicPipelineLayout;
        lveGraphicPipeline = std::make_unique<LveGraphicPipeline>(
            lveDevice,
            graphicPipelineConfigInfo);
    }

} // namespace lve
