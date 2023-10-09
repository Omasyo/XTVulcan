// #include <vulkan/vulkan_raii.hpp>

#include <vulkan/vulkan_core.h> //TODO: Not sure why this isn't added in raii, since it looks like it's actually there

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <iostream>
#include <set>
#include <unordered_map>

#include "application.hpp"

const std::unordered_map<VkDebugUtilsMessageSeverityFlagBitsEXT, const char *> severityTag = {
    {VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "E"},
    {VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, "I"},
    {VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, "V"},
    {VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, "W"}};

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    std::cerr << "ValidationLayer\\" << severityTag.at(messageSeverity) << ": " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo)
{
    using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageType = vk::DebugUtilsMessageTypeFlagBitsEXT;

    createInfo = vk::DebugUtilsMessengerCreateInfoEXT{
        .sType = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT,
        .messageSeverity = Severity::eVerbose | Severity::eWarning | Severity::eWarning | Severity::eInfo,
        .messageType = MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
        .pfnUserCallback = debugCallback};
}

void Application::run()
{
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void Application::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "XTVulkan", nullptr, nullptr);
}

void Application::initVulkan()
{
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
}

void Application::mainLoop()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        drawFrame();
    }
    // device.waitIdle();
}

void Application::cleanup()
{
    glfwDestroyWindow(window);

    glfwTerminate();
}

void Application::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    vk::ApplicationInfo appInfo{
        .sType = vk::StructureType::eApplicationInfo,
        .pApplicationName = "XTVulcan",
        .applicationVersion = 1,
        .pEngineName = "No Engine",
        .engineVersion = 1,
        .apiVersion = VK_API_VERSION_1_0};

    vk::InstanceCreateInfo createInfo{
        .sType = vk::StructureType::eInstanceCreateInfo,
        .pApplicationInfo = &appInfo};

    auto extensions = getRequiredExtensions();

    createInfo.setEnabledExtensionCount(extensions.size())
        .setPpEnabledExtensionNames(extensions.data());

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers)
    {
        createInfo.setEnabledLayerCount(validationLayers.size())
            .setPpEnabledLayerNames(validationLayers.data());

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.setPNext(&debugCreateInfo);
    }

    instance = context.createInstance(createInfo);

    // for (const auto &extension : vk::enumerateInstanceExtensionProperties())
    // {
    //     std::cout << '\t' << extension.extensionName << '\n';
    // }
}

bool Application::checkValidationLayerSupport()
{
    auto availableLayers = vk::enumerateInstanceLayerProperties();
    for (auto layer : validationLayers)

    {
        std::cout << '\t' << "Checking for layer " << layer << std::endl;
        if (std::ranges::find_if(availableLayers, [layer](auto properties)
                                 { 
        std::cout << '\t'  << "Comaparing " << properties.layerName << " with " << layer << std::endl;
                            return strcmp(layer, properties.layerName) == 0; }) == availableLayers.cend())
        {
            std::cout << '\t' << layer << " not found" << std::endl;
            return false;
        }
    }

    return true;
}

std::vector<const char *> Application::getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Application::setupDebugMessenger()
{
    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
}

int rateDeviceSuitability(vk::raii::PhysicalDevice device)
{

    auto deviceProperties = device.getProperties();
    auto deviceFeatures = device.getFeatures();

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
    {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    // Application can't function without geometry shaders
    if (!deviceFeatures.geometryShader)
    {
        return 0;
    }

    return score;
}

void Application::pickPhysicalDevice()
{
    auto devices = instance.enumeratePhysicalDevices();
    // if (devices.empty())
    // {
    //     throw std::runtime_error("failed to find GPUs with Vulkan support!");
    // }

    // for (const auto &device : devices)
    // {
    //     if (isDeviceSuitable(device))
    //     {
    //         physicalDevice = device;
    //         break;
    //     }
    // }

    std::multimap<int, vk::raii::PhysicalDevice> candidates;

    for (const auto &device : devices)
    {
        int score = rateDeviceSuitability(device);
        candidates.insert(std::make_pair(score, device));
    }

    // Check if the best candidate is suitable at all
    if (candidates.rbegin()->first > 0)
    {
        physicalDevice = candidates.rbegin()->second;
    }
    else
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    if (!*physicalDevice)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool Application::isDeviceSuitable(vk::raii::PhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool Application::checkDeviceExtensionSupport(vk::raii::PhysicalDevice device)
{
    std::set<std::string> requiredExtensions(deviceExtensions.cbegin(), deviceExtensions.cend());

    for (const auto &extension : device.enumerateDeviceExtensionProperties())
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices Application::findQueueFamilies(vk::raii::PhysicalDevice device)
{
    QueueFamilyIndices indices;

    auto queueFamilies = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto &queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphicsFamily = i;
        }

        vk::Bool32 presentSupport = false;
        if (device.getSurfaceSupportKHR(i, *surface))
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

void Application::createLogicalDevice()
{
    auto indices = findQueueFamilies(physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queryPriority = 1.0f;
    for (auto queueFamily : uniqueQueueFamilies)
    {
        vk::DeviceQueueCreateInfo queueCreateInfo{
            .sType = vk::StructureType::eDeviceQueueCreateInfo,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queryPriority,
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::DeviceCreateInfo deviceCreateInfo{
        .sType = vk::StructureType::eDeviceCreateInfo,
        .queueCreateInfoCount = queueCreateInfos.size(),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = deviceExtensions.size(),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    if (enableValidationLayers)
    {
        deviceCreateInfo
            .setEnabledLayerCount(validationLayers.size())
            .setPpEnabledLayerNames(validationLayers.data());
    }
    device = physicalDevice.createDevice(deviceCreateInfo);

    graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
    presentQueue = device.getQueue(indices.presentFamily.value(), 0);
}

void Application::createSurface()
{
    auto surfaceHandle = VkSurfaceKHR();

    if (glfwCreateWindowSurface(*instance, window, nullptr, &surfaceHandle) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
    // surface = vk::raii::SurfaceKHR(instance, surfaceHandle);
    surface = vk::raii::SurfaceKHR(instance, surfaceHandle);
}

SwapChainSupportDetails Application::querySwapChainSupport(vk::raii::PhysicalDevice device)
{

    return SwapChainSupportDetails{
        .capabilities = device.getSurfaceCapabilitiesKHR(*surface),
        .formats = device.getSurfaceFormatsKHR(*surface),
        .presentModes = device.getSurfacePresentModesKHR(*surface),
    };
}

vk::SurfaceFormatKHR Application::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
{
    for (const auto &availableFormat : availableFormats)
    {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

vk::PresentModeKHR Application::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes)
{
    for (const auto &availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
        {
            return availablePresentMode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Application::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return vk::Extent2D{
            .width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            .height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
        };
    }
}

void Application::createSwapChain()
{
    auto swapChainSupport = querySwapChainSupport(physicalDevice);

    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    auto extent = chooseSwapExtent(swapChainSupport.capabilities);

    vk::SwapchainCreateInfoKHR createInfo{
        .sType = vk::StructureType::eSwapchainCreateInfoKHR,
        .surface = *surface,
        .minImageCount = std::min(swapChainSupport.capabilities.minImageCount + 1, swapChainSupport.capabilities.maxImageCount), // TODO can maxCOunt be < 0
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
    };

    auto indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndexCount(2)
            .setPQueueFamilyIndices(queueFamilyIndices);
    }
    else
    {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    createInfo.setPreTransform(swapChainSupport.capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(presentMode)
        .setClipped(VK_TRUE)
        .setOldSwapchain(VK_NULL_HANDLE);

    swapChain = device.createSwapchainKHR(createInfo);

    swapChainImages = swapChain.getImages();

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void Application::createImageViews()
{
    for (auto &&image : swapChainImages)
    {
        vk::ImageViewCreateInfo createInfo{
            .sType = vk::StructureType::eImageViewCreateInfo,
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = swapChainImageFormat,
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

        swapChainImageViews.push_back(device.createImageView(createInfo));
    }
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

void Application::createGraphicsPipeline()
{
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    auto vertShaderModule = createShaderModule(vertShaderCode);
    auto fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .sType = vk::StructureType::ePipelineShaderStageCreateInfo,
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = *vertShaderModule,
        .pName = "main",
    };

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .sType = vk::StructureType::ePipelineShaderStageCreateInfo,
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = *fragShaderModule,
        .pName = "main",
    };

    vk::PipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo,
    };

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .sType = vk::StructureType::ePipelineDynamicStateCreateInfo,
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data(),
    };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = vk::StructureType::ePipelineVertexInputStateCreateInfo,
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
        .sType = vk::StructureType::ePipelineInputAssemblyStateCreateInfo,
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = VK_FALSE,
    };

    vk::PipelineViewportStateCreateInfo viewportInfo{
        .sType = vk::StructureType::ePipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizerInfo{
        .sType = vk::StructureType::ePipelineRasterizationStateCreateInfo,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampleInfo{
        .sType = vk::StructureType::ePipelineMultisampleStateCreateInfo,
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = vk::BlendFactor::eOne,
        .dstColorBlendFactor = vk::BlendFactor::eZero,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vk::ColorComponentFlagBits::eR |
                          vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB |
                          vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo colorBlendInfo{
        .sType = vk::StructureType::ePipelineColorBlendStateCreateInfo,
        .logicOpEnable = VK_FALSE,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = vk::StructureType::ePipelineLayoutCreateInfo,
    };

    pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .sType = vk::StructureType::eGraphicsPipelineCreateInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pViewportState = &viewportInfo,
        .pRasterizationState = &rasterizerInfo,
        .pMultisampleState = &multisampleInfo,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &colorBlendInfo,
        .pDynamicState = &dynamicState,
        .layout = *pipelineLayout,
        .renderPass = *renderPass,
        .subpass = 0,
    };

    graphicsPipeline = device.createGraphicsPipeline(nullptr, pipelineInfo);
}

vk::raii::ShaderModule Application::createShaderModule(const std::vector<char> &code)
{
    vk::ShaderModuleCreateInfo createInfo{
        .sType = vk::StructureType::eShaderModuleCreateInfo,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t *>(code.data()),
    };

    return device.createShaderModule(createInfo);
}

void Application::createRenderPass()
{
    vk::AttachmentDescription colorAttachment{
        .format = swapChainImageFormat,
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
        .pColorAttachments = &colorAttachmentRef};

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
    };

    vk::RenderPassCreateInfo renderPassInfo{
        .sType = vk::StructureType::eRenderPassCreateInfo,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    renderPass = device.createRenderPass(renderPassInfo);
}

void Application::createFramebuffers()
{
    swapChainFramebuffers.clear();
    for (auto &&imageView : swapChainImageViews)
    {
        std::array attachments{
            *imageView,
        };

        vk::FramebufferCreateInfo frameBufferInfo{
            .sType = vk::StructureType::eFramebufferCreateInfo,
            .renderPass = *renderPass,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .width = swapChainExtent.width,
            .height = swapChainExtent.height,
            .layers = 1,
        };

        swapChainFramebuffers.push_back(device.createFramebuffer(frameBufferInfo));
    }
}

void Application::createCommandPool()
{
    auto queueFamilyIndices = findQueueFamilies(physicalDevice);

    vk::CommandPoolCreateInfo poolInfo{
        .sType = vk::StructureType::eCommandPoolCreateInfo,
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
    };

    commandPool = device.createCommandPool(poolInfo);
}

void Application::createCommandBuffer()
{
    vk::CommandBufferAllocateInfo allocateInfo{
        .sType = vk::StructureType::eCommandBufferAllocateInfo,
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    // TODO could be vector device.alloca... returns vector ?
    commandBuffers = vk::raii::CommandBuffers(device, allocateInfo);
}

void Application::recordCommandBuffer(const vk::raii::CommandBuffer &commandBuffer, uint32_t imageIndex)
{
    vk::CommandBufferBeginInfo beginInfo{
        .sType = vk::StructureType::eCommandBufferBeginInfo,
    };

    commandBuffer.begin(beginInfo);

    vk::ClearValue clearColor({{{0.0f, 0.0f, 0.0f, 1.0f}}});
    vk::RenderPassBeginInfo renderPassInfo{
        .sType = vk::StructureType::eRenderPassBeginInfo,
        .renderPass = *renderPass,
        .framebuffer = *swapChainFramebuffers[imageIndex],
        .renderArea = {
            .offset = {0, 0},
            .extent = swapChainExtent,
        },
        .clearValueCount = 1,
        .pClearValues = &clearColor,
    };

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

    vk::Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = swapChainExtent.width,
        .height = swapChainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    commandBuffer.setViewport(0, viewport);

    vk::Rect2D scissor{
        .offset = {0, 0},
        .extent = swapChainExtent,
    };
    commandBuffer.setScissor(0, scissor);

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRenderPass();

    commandBuffer.end();
}

void Application::drawFrame()
{
    if(device.waitForFences(*inFlightFence, VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
        std::clog << "DrawFrame:\tCould not wait for fences\n";
    }
    device.resetFences(*inFlightFence);

    // device.acquireNextImage2KHR({
    //     .sType = vk::StructureType::eAcquireNextImageInfoKHR,
    //     .swapchain = *swapChain,
    //     .timeout = UINT64_MAX,
    // });

    // TODO: change this
    uint32_t imageIndex;

    if (vkAcquireNextImageKHR(*device, *swapChain, UINT64_MAX, *imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex) != VK_SUCCESS)
    {
        std::clog << "DrawFrame:\tCould not present\n";
    }

    auto &commandBuffer = commandBuffers.front();
    commandBuffer.reset();

    recordCommandBuffer(commandBuffer, imageIndex);

    std::array waitSephamores{
        *imageAvailableSemaphore,
    };
    std::array<vk::PipelineStageFlags, waitSephamores.size()> waitStages{
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
    };
    // TODO: assert that their sizes are the same
    std::array signalSephamores{
        *renderFinishedSemaphore,
    };
    
    vk::CommandBuffer t[commandBuffers.size()];
    //todo change this
    for (size_t i = 0; i < commandBuffers.size(); ++i)
    {
        t[i] = *commandBuffers[i];
    }
    

    vk::SubmitInfo submitInfo{
        .sType = vk::StructureType::eSubmitInfo,
        .waitSemaphoreCount = waitSephamores.size(),
        .pWaitSemaphores = waitSephamores.data(),
        .pWaitDstStageMask = waitStages.data(),
        .commandBufferCount = commandBuffers.size(),
        .pCommandBuffers = t, // I don't know what I'm doing
        .signalSemaphoreCount = signalSephamores.size(),
        .pSignalSemaphores = signalSephamores.data(),
    };

    graphicsQueue.submit(submitInfo, *inFlightFence);

    std::array swapchains = {*swapChain};
    vk::PresentInfoKHR presentInfo{
        .sType = vk::StructureType::ePresentInfoKHR,
        .waitSemaphoreCount = signalSephamores.size(),
        .pWaitSemaphores = signalSephamores.data(),
        .swapchainCount = swapchains.size(),
        .pSwapchains = swapchains.data(),
        .pImageIndices = &imageIndex};

    if (presentQueue.presentKHR(presentInfo) != vk::Result::eSuccess)
    {
        std::clog << "DrawFrame:\tCould not present\n";
    }
}

void Application::createSyncObjects()
{

    imageAvailableSemaphore = device.createSemaphore({
        .sType = vk::StructureType::eSemaphoreCreateInfo,
    });
    renderFinishedSemaphore = device.createSemaphore({
        .sType = vk::StructureType::eSemaphoreCreateInfo,
    });

    inFlightFence = device.createFence({
        .sType = vk::StructureType::eFenceCreateInfo,
        .flags = vk::FenceCreateFlagBits::eSignaled,
    });
}