#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const char *APP_NAME = "Hello Triangle";

const int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct QueueFamilyIndices
{
    uint32_t graphicsFamily;
    uint32_t presentFamily;
};

struct UniformBufferObject
{
   alignas(16) glm::mat4 model;
   alignas(16) glm::mat4 view;
   alignas(16) glm::mat4 proj;
};

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static std::array<vk::VertexInputBindingDescription, 1> getBindingDescriptions()
    {
        return {vk::VertexInputBindingDescription{
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex,
        }};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        vk::VertexInputAttributeDescription posDescription{
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = offsetof(Vertex, pos),
        };
        vk::VertexInputAttributeDescription colorDescription{
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, color),
        };
        return std::array{posDescription, colorDescription};
    }
};

bool checkExtensionSupport(std::vector<const char *> requiredExtensions, std::vector<vk::ExtensionProperties> properties)
{
    std::unordered_set<std::string> extensionSet(requiredExtensions.cbegin(), requiredExtensions.cend());

    for (auto property : properties)
    {
        extensionSet.erase(property.extensionName);
    }

    return extensionSet.empty();
}

static std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

class Application
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    const std::vector<Vertex> vertices{
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
    };

    const std::vector<uint16_t> indices{
        0, 1, 2, 2, 3, 0};

    GLFWwindow *window;

    vk::raii::Context context;
    vk::raii::Instance instance{nullptr};

    vk::raii::SurfaceKHR surface{nullptr};

    vk::raii::PhysicalDevice physicalDevice{nullptr};

    vk::raii::Device logicalDevice{nullptr};
    vk::raii::Queue graphicsQueue{nullptr};
    vk::raii::Queue presentQueue{nullptr};

    QueueFamilyIndices queueFamilyIndices;
    vk::Extent2D swapchainExtent;
    vk::Format swapchainFormat;
    // vk::SurfaceCapabilitiesKHR capabilities;
    vk::raii::SwapchainKHR swapchain{nullptr};
    std::vector<vk::raii::ImageView> swapchainImageViews;

    vk::raii::RenderPass renderPass{nullptr};
    vk::raii::DescriptorSetLayout descriptorSetLayout{nullptr};
    vk::raii::PipelineLayout pipelineLayout{nullptr};
    vk::raii::Pipeline graphicsPipeline{nullptr};

    std::vector<vk::raii::Framebuffer> swapchainFrameBuffers;

    vk::raii::CommandPool commandPool{nullptr};
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    // std::array<vk::raii::CommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers;

    vk::raii::Buffer vertexBuffer{nullptr};
    vk::raii::DeviceMemory vertexBufferMemory{nullptr};

    vk::raii::Buffer indexBuffer{nullptr};
    vk::raii::DeviceMemory indexBufferMemory{nullptr};

    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;

    vk::raii::DescriptorPool descriptorPool{nullptr};
    std::vector<vk::raii::DescriptorSet> descriptorSets;

    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;

    uint32_t currentFrame = 0;

    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
        createSurface();

        pickPhysicalDevice();

        createLogicalDevice();

        graphicsQueue = logicalDevice.getQueue(queueFamilyIndices.graphicsFamily, 0);
        presentQueue = logicalDevice.getQueue(queueFamilyIndices.presentFamily, 0);

        createSwapchain();
        createRenderPass();

        createDescriptorSetLayout();
        createGraphicsPipeline();

        createFrameBuffers();

        commandPool = logicalDevice.createCommandPool({
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueFamilyIndices.graphicsFamily,
        });

        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();

        createDescriptorPool();
        createDescriptorSets();

        commandBuffers = std::move(logicalDevice.allocateCommandBuffers({
            .commandPool = *commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        }));

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {

            imageAvailableSemaphores.push_back(logicalDevice.createSemaphore({}));
            renderFinishedSemaphores.push_back(logicalDevice.createSemaphore({}));
            inFlightFences.push_back(logicalDevice.createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));
        }
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }
        logicalDevice.waitIdle();
    }

    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance()
    {
        auto availableLayers = context.enumerateInstanceLayerProperties();
        bool allLayersSupported = true;

        for (auto layer : validationLayers)
        {
            if (std::ranges::find_if(availableLayers,
                                     [layer](auto properties)
                                     { return strcmp(layer, properties.layerName) == 0; }) ==
                availableLayers.cend())
            {
                std::cerr << '\t' << layer << " not found" << std::endl;
                allLayersSupported = false;
            }
        }

        if (enableValidationLayers && !allLayersSupported)
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        vk::ApplicationInfo appInfo{
            .pApplicationName = APP_NAME,
            .pEngineName = APP_NAME,
            .apiVersion = VK_API_VERSION_1_0,
        };

        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions,
        };

        if (enableValidationLayers)
        {
            createInfo.setPEnabledLayerNames(validationLayers);
        }

        instance = context.createInstance(createInfo);
    }

    void createSurface()
    {

        auto surfaceHandle = VkSurfaceKHR();

        if (glfwCreateWindowSurface(*instance, window, nullptr, &surfaceHandle) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, surfaceHandle);
    }

    void pickPhysicalDevice()
    {
        auto physicalDevices = instance.enumeratePhysicalDevices();

        // TODO: Flicker probably caused by Nvidia being used with Wayland
        // Find descrete GPU that supports required device extensions
        // for (const auto &device : physicalDevices)
        // {
        //     auto properties = device.enumerateDeviceExtensionProperties();
        //     if (
        //         (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) &&
        //         (checkExtensionSupport(deviceExtensions, properties)))
        //     {
        //         physicalDevice = device;
        //         return;
        //     }
        // }

        // Find any GPU that supports required device extensions
        for (const auto &device : physicalDevices)
        {
            auto properties = device.enumerateDeviceExtensionProperties();
            if (checkExtensionSupport(deviceExtensions, properties))
            {
                physicalDevice = device;
                return;
            }
        }

        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    void createLogicalDevice()
    {
        auto properties = physicalDevice.getQueueFamilyProperties();

        queueFamilyIndices = getQueueFamilyIndices(properties);
        // queueFamilyIndices.presentFamily = getQueueFamilyIndex(properties, vk::QueueFlagBits::eTransfer);

        float priority = 1.0f;

        std::unordered_set<uint32_t> uniqueQueueFamilies = {queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily};
        std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;

        for (auto queueFamily : uniqueQueueFamilies)
        {
            deviceQueueCreateInfos.push_back({
                .queueFamilyIndex = queueFamilyIndices.graphicsFamily,
                .queueCount = 1,
                .pQueuePriorities = &priority,
            });
        }

        vk::PhysicalDeviceFeatures deviceFeatures{};

        vk::DeviceCreateInfo deviceCreateInfo{
            .queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size()),
            .pQueueCreateInfos = deviceQueueCreateInfos.data(),
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &deviceFeatures,
        };

        if (enableValidationLayers)
        {
            deviceCreateInfo.setPEnabledLayerNames(validationLayers);
        }
        logicalDevice = physicalDevice.createDevice(deviceCreateInfo);
    }

    QueueFamilyIndices getQueueFamilyIndices(
        const std::vector<vk::QueueFamilyProperties> &properties)
    {
        QueueFamilyIndices indices{UINT32_MAX, UINT32_MAX};
        for (uint32_t i = 0; i < properties.size(); ++i)
        {
            if ((properties[i].queueFlags & vk::QueueFlagBits::eGraphics))
            {
                indices.graphicsFamily = i;
            }

            if (physicalDevice.getSurfaceSupportKHR(i, *surface))
            {
                indices.presentFamily = i;
            }

            if (indices.graphicsFamily != UINT32_MAX && indices.presentFamily != UINT32_MAX)
            {
                return indices;
            }
        }

        throw std::runtime_error("Could not find a matching queue family index");
    }

    void createSwapchain()
    {
        auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        auto formats = physicalDevice.getSurfaceFormatsKHR(*surface);
        auto presentModes = physicalDevice.getSurfacePresentModesKHR(*surface);

        auto surfaceFormat = formats[0];
        for (auto &&availableFormat : formats)
        {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                surfaceFormat = availableFormat;
            }
        }
        swapchainFormat = surfaceFormat.format; // TODO remove this

        auto presentMode = vk::PresentModeKHR::eFifo;

        swapchainExtent = capabilities.currentExtent;
        if (swapchainExtent.width == std::numeric_limits<uint32_t>::max())
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            swapchainExtent = vk::Extent2D{
                .width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                .height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
            };
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo{
            .surface = *surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = swapchainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = *swapchain};

        std::array indices = {queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily};

        if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily)
        {
            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndices(indices);
        }
        else
        {
            createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
        }
        swapchain = logicalDevice.createSwapchainKHR(createInfo);

        auto swapchainImages = swapchain.getImages();

        swapchainImageViews.clear();
        swapchainImageViews.reserve(swapchainImages.size());
        for (auto &&image : swapchainImages)
        {
            vk::ImageViewCreateInfo createInfo{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = surfaceFormat.format,
                .components = {
                    .r = vk::ComponentSwizzle::eIdentity,
                    .g = vk::ComponentSwizzle::eIdentity,
                    .b = vk::ComponentSwizzle::eIdentity,
                    .a = vk::ComponentSwizzle::eIdentity,
                },
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            swapchainImageViews.push_back(logicalDevice.createImageView(createInfo));
        }
    }

    void createRenderPass()
    {
        vk::AttachmentDescription colorAttachment{
            .format = swapchainFormat,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR,
        };

        vk::AttachmentReference colorAttachmentRef{
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
        };

        vk::SubpassDescription subpass{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef,
        };

        vk::SubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            // .srcAccessMask = 0,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite};

        renderPass = logicalDevice.createRenderPass({
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        });
    }

    void createGraphicsPipeline()
    {
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");

        auto vertShaderModule = logicalDevice.createShaderModule({
            .codeSize = vertShaderCode.size(),
            .pCode = reinterpret_cast<const uint32_t *>(vertShaderCode.data()),
        });
        auto fragShaderModule = logicalDevice.createShaderModule({
            .codeSize = fragShaderCode.size(),
            .pCode = reinterpret_cast<const uint32_t *>(fragShaderCode.data()),
        });

        std::array shaderStages{
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = *vertShaderModule,
                .pName = "main",
            },
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = *fragShaderModule,
                .pName = "main",
            },
        };

        std::array dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };

        vk::PipelineDynamicStateCreateInfo dynamicStateInfo{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };

        auto bindingDescriptions = Vertex::getBindingDescriptions();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInputStateInfo{
            .vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size()),
            .pVertexBindingDescriptions = bindingDescriptions.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data(),
        };

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE,
        };

        vk::PipelineViewportStateCreateInfo viewportStateInfo{
            .sType = vk::StructureType::ePipelineViewportStateCreateInfo,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        vk::PipelineRasterizationStateCreateInfo rasterizationStateInfo{
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f,
        };

        vk::PipelineMultisampleStateCreateInfo multisampleStateInfo{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE,
        };

        vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };

        vk::PipelineColorBlendStateCreateInfo colorBlendingStateInfo{
            .logicOpEnable = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachmentState,
        };

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount = 1,
            .pSetLayouts = &*descriptorSetLayout,
        };

        pipelineLayout = logicalDevice.createPipelineLayout(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo{
            .stageCount = static_cast<uint32_t>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputStateInfo,
            .pInputAssemblyState = &inputAssemblyStateInfo,
            .pViewportState = &viewportStateInfo,
            .pRasterizationState = &rasterizationStateInfo,
            .pMultisampleState = &multisampleStateInfo,
            .pColorBlendState = &colorBlendingStateInfo,
            .pDynamicState = &dynamicStateInfo,
            .layout = *pipelineLayout,
            .renderPass = *renderPass,
            .subpass = 0};

        graphicsPipeline = logicalDevice.createGraphicsPipeline(nullptr, pipelineInfo, nullptr);
    }

    void createFrameBuffers()
    {
        // TODO do i clear swapchainbuffers first
        swapchainFrameBuffers.clear();
        swapchainFrameBuffers.reserve(swapchainImageViews.size());

        for (auto &&imageViews : swapchainImageViews)
        {
            std::array attachments{*imageViews};

            swapchainFrameBuffers.push_back(
                logicalDevice.createFramebuffer({.renderPass = *renderPass,
                                                 .attachmentCount = static_cast<uint32_t>(attachments.size()),
                                                 .pAttachments = attachments.data(),
                                                 .width = swapchainExtent.width,
                                                 .height = swapchainExtent.height,
                                                 .layers = 1}));
        }
    }

    void recordCommandBuffer(const vk::raii::CommandBuffer &commandBuffer, uint32_t imageIndex)
    {
        commandBuffer.begin({});

        vk::ClearValue clearColor({{{0.0f, 0.0f, 0.0f, 1.0f}}});

        vk::RenderPassBeginInfo renderPassInfo{
            .renderPass = *renderPass,
            .framebuffer = *swapchainFrameBuffers[imageIndex],
            .renderArea = {
                .offset = {0, 0},
                .extent = swapchainExtent,
            },
            .clearValueCount = 1,
            .pClearValues = &clearColor};

        commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

        vk::Viewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapchainExtent.width),
            .height = static_cast<float>(swapchainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        commandBuffer.setViewport(0, viewport);

        vk::Rect2D scissor{
            .offset = {0, 0},
            .extent = swapchainExtent,
        };
        commandBuffer.setScissor(0, scissor);

        commandBuffer.bindVertexBuffers(0, {*vertexBuffer}, {0});
        commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipelineLayout, 0, {*descriptorSets[currentFrame]}, nullptr);

        commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        commandBuffer.endRenderPass();

        commandBuffer.end();
    }

    void drawFrame()
    {
        if (logicalDevice.waitForFences(*inFlightFences[currentFrame], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
        {
            std::cerr << "DrawFrame:\tCould not wait for fences\n";
        }

        auto [result, imageIndex] = swapchain.acquireNextImage(UINT64_MAX, *imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE);

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapchain();
            return;
        }
        else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        logicalDevice.resetFences(*inFlightFences[currentFrame]);

        commandBuffers[currentFrame].reset();
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        std::array waitSemaphores{*imageAvailableSemaphores[currentFrame]};
        std::array<vk::PipelineStageFlags, waitSemaphores.size()> waitStages{vk::PipelineStageFlagBits::eColorAttachmentOutput};

        std::array signalSemaphores{*renderFinishedSemaphores[currentFrame]};

        updateUniformBuffer(currentFrame);

        vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
            .pWaitSemaphores = waitSemaphores.data(),
            .pWaitDstStageMask = waitStages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffers[currentFrame],
            .signalSemaphoreCount = signalSemaphores.size(),
            .pSignalSemaphores = signalSemaphores.data(),
        };

        graphicsQueue.submit({submitInfo}, *inFlightFences[currentFrame]);

        std::array swapchains = {*swapchain};
        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = signalSemaphores.size(),
            .pWaitSemaphores = signalSemaphores.data(),
            .swapchainCount = swapchains.size(),
            .pSwapchains = swapchains.data(),
            .pImageIndices = &imageIndex,
        };
        VkPresentInfoKHR info = presentInfo;

        result = vk::Result(vkQueuePresentKHR(*presentQueue, &info));
        // result = presentQueue.presentKHR(presentInfo);
        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
        {
            recreateSwapchain();
        }
        else if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void recreateSwapchain()
    {
        logicalDevice.waitIdle();

        createSwapchain();
        createFrameBuffers();
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        auto memoryProperties = physicalDevice.getMemoryProperties();

        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("Failed to find a suitable memory type!");
    }

    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties)
    {
        auto buffer = logicalDevice.createBuffer({
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
        });

        auto memoryRequirements = buffer.getMemoryRequirements();

        auto memory = logicalDevice.allocateMemory({
            .allocationSize = size,
            .memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties),
        });

        buffer.bindMemory(*memory, 0);

        return std::make_pair(std::move(buffer), std::move(memory));
    }

    void copyBuffer(const vk::raii::Buffer &srcBuffer, const vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
    {
        vk::CommandBufferAllocateInfo allocateInfo{
            .commandPool = *commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };

        auto commandBuffers = logicalDevice.allocateCommandBuffers(allocateInfo);
        auto &commandBuffer = commandBuffers.front();

        commandBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        commandBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{.size = size});
        commandBuffer.end();

        graphicsQueue.submit({vk::SubmitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer,
        }});
        graphicsQueue.waitIdle();
    }

    void createVertexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        auto data = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, vertices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(vertexBuffer, vertexBufferMemory) = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    }

    void createIndexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        auto data = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, indices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(indexBuffer, indexBufferMemory) = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    }

    void createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
        };

        descriptorSetLayout = logicalDevice.createDescriptorSetLayout({
            .bindingCount = 1,
            .pBindings = &uboLayoutBinding,
        });
    }

    void createUniformBuffers()
    {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            auto [buffer, bufferMemory] = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            uniformBuffersMapped.push_back(bufferMemory.mapMemory(0, bufferSize));

            uniformBuffers.push_back(std::move(buffer));
            uniformBuffersMemory.push_back(std::move(bufferMemory));
        }
    }

    void updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{
            .model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .proj = glm::perspective(glm::radians(45.0f), swapchainExtent.width / static_cast<float>(swapchainExtent.height), 0.1f, 10.0f)};

        ubo.proj[1][1] *= -1;
        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void createDescriptorPool()
    {
        vk::DescriptorPoolSize poolSize{
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
            };

        descriptorPool = logicalDevice.createDescriptorPool({
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        });
    }

    void createDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

        descriptorSets = logicalDevice.allocateDescriptorSets({
            .descriptorPool = *descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
        });

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vk::DescriptorBufferInfo bufferInfo{
                .buffer = *uniformBuffers[i],
                .offset = 0,
                .range = sizeof(UniformBufferObject),
            };

            vk::WriteDescriptorSet descriptorWrite{
                .dstSet = *descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &bufferInfo,
            };

            logicalDevice.updateDescriptorSets({descriptorWrite}, nullptr);
        }
    }
};