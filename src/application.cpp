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

vk::DebugUtilsMessengerCreateInfoEXT createDebugMessengerCreateInfo()
{
    using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageType = vk::DebugUtilsMessageTypeFlagBitsEXT;

    return vk::DebugUtilsMessengerCreateInfoEXT{
        .sType = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT,
        .messageSeverity = Severity::eVerbose | Severity::eWarning | Severity::eWarning | Severity::eInfo,
        .messageType = MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
        .pfnUserCallback = debugCallback};
}

std::vector<const char *> getRequiredExtensions()
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

bool checkValidationLayerSupport()
{
    auto availableLayers = vk::enumerateInstanceLayerProperties();
    for (auto layer : validationLayers)
    {
        if (std::ranges::find_if(availableLayers,
                                 [layer](auto properties)
                                 { return strcmp(layer, properties.layerName) == 0; }) ==
            availableLayers.cend())
        {
            std::cerr << '\t' << layer << " not found" << std::endl;
            return false;
        }
    }

    return true;
}

vk::raii::Instance createInstance(const vk::raii::Context &context)
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

    if (enableValidationLayers)
    {
        createInfo.setEnabledLayerCount(validationLayers.size())
            .setPpEnabledLayerNames(validationLayers.data());

        auto debugCreateInfo = createDebugMessengerCreateInfo();
        createInfo.setPNext(&debugCreateInfo);
    }

    return context.createInstance(createInfo);
}

vk::raii::DebugUtilsMessengerEXT createDebugMessenger(const vk::raii::Instance &instance)
{
    return instance.createDebugUtilsMessengerEXT(createDebugMessengerCreateInfo());
}

vk::raii::SurfaceKHR createSurface(const vk::raii::Instance &instance, GLFWwindow *const window)
{
    auto surfaceHandle = VkSurfaceKHR();

    if (glfwCreateWindowSurface(*instance, window, nullptr, &surfaceHandle) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
    return vk::raii::SurfaceKHR(instance, surfaceHandle);
}

QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice &device, const vk::raii::SurfaceKHR &surface)
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

SwapChainSupportDetails querySwapChainSupport(const vk::raii::PhysicalDevice &device, const vk::raii::SurfaceKHR &surface)
{
    return SwapChainSupportDetails{
        .capabilities = device.getSurfaceCapabilitiesKHR(*surface),
        .formats = device.getSurfaceFormatsKHR(*surface),
        .presentModes = device.getSurfacePresentModesKHR(*surface),
    };
}

int rateDeviceSuitability(const vk::raii::PhysicalDevice &device)
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

bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice &device)
{
    std::set<std::string> requiredExtensions(deviceExtensions.cbegin(), deviceExtensions.cend());

    for (const auto &extension : device.enumerateDeviceExtensionProperties())
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool isDeviceSuitable(const vk::raii::PhysicalDevice &device, const vk::raii::SurfaceKHR &surface)
{
    QueueFamilyIndices indices = findQueueFamilies(device, surface);

    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

vk::raii::PhysicalDevice pickPhysicalDevice(const vk::raii::Instance &instance, const vk::raii::SurfaceKHR &surface)
{
    auto devices = instance.enumeratePhysicalDevices();
    if (devices.empty())
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    for (const auto &device : devices)
    {
        if (isDeviceSuitable(device, surface))
        {
            return device;
        }
    }

    throw std::runtime_error("failed to find a suitable GPU!");
}

vk::raii::Device createLogicalDevice(const vk::raii::PhysicalDevice &physicalDevice, const QueueFamilyIndices &indices)
{
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());

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

    auto deviceFeatures = physicalDevice.getFeatures();
    vk::DeviceCreateInfo deviceCreateInfo{
        .sType = vk::StructureType::eDeviceCreateInfo,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    if (enableValidationLayers)
    {
        deviceCreateInfo
            .setEnabledLayerCount(validationLayers.size())
            .setPpEnabledLayerNames(validationLayers.data());
    }
    return physicalDevice.createDevice(deviceCreateInfo);
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
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

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes)
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

vk::Extent2D chooseSwapExtent(GLFWwindow *const window, const vk::SurfaceCapabilitiesKHR &capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        std::clog << width << " " << height << std::endl;

        return vk::Extent2D{
            .width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            .height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
        };
    }
}

vk::raii::SwapchainKHR createSwapChain(
    const vk::raii::Device &device,
    const vk::raii::SurfaceKHR &surface,
    const vk::SurfaceFormatKHR &surfaceFormat,
    const vk::Extent2D &extent,
    const SwapChainSupportDetails &swapchainSupport,
    const QueueFamilyIndices &indices)
{
    auto presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);

    vk::SwapchainCreateInfoKHR createInfo{
        .sType = vk::StructureType::eSwapchainCreateInfoKHR,
        .surface = *surface,
        .minImageCount = std::min(swapchainSupport.capabilities.minImageCount + 1, swapchainSupport.capabilities.maxImageCount), // TODO can maxCOunt be < 0
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
    };

    std::array queueFamilyIndices{indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndexCount(queueFamilyIndices.size())
            .setPQueueFamilyIndices(queueFamilyIndices.data());
    }
    else
    {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    createInfo.setPreTransform(swapchainSupport.capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(presentMode)
        .setClipped(VK_TRUE)
        .setOldSwapchain(VK_NULL_HANDLE);

    return device.createSwapchainKHR(createInfo);
}

std::vector<vk::raii::ImageView> createImageViews(
    const vk::raii::Device &device,
    const std::vector<vk::Image> &swapchainImages,
    const vk::Format &swapchainImageFormat)
{
    std::vector<vk::raii::ImageView> imageViews;
    imageViews.reserve(swapchainImages.size());
    for (auto &&image : swapchainImages)
    {
        vk::ImageViewCreateInfo createInfo{
            .sType = vk::StructureType::eImageViewCreateInfo,
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = swapchainImageFormat,
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
        imageViews.push_back(device.createImageView(createInfo));
    }
    return imageViews;
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

vk::raii::ShaderModule createShaderModule(const vk::raii::Device &device, const std::vector<char> &code)
{
    vk::ShaderModuleCreateInfo createInfo{
        .sType = vk::StructureType::eShaderModuleCreateInfo,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t *>(code.data()),
    };

    return device.createShaderModule(createInfo);
}

vk::raii::Pipeline createGraphicsPipeline(const vk::raii::Device &device, const vk::raii::RenderPass &renderPass)
{
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    auto vertShaderModule = createShaderModule(device, vertShaderCode);
    auto fragShaderModule = createShaderModule(device, fragShaderCode);

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
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
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

    auto pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

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

    return device.createGraphicsPipeline(nullptr, pipelineInfo);
}

vk::raii::RenderPass createRenderPass(const vk::raii::Device &device, const vk::Format &swapchainImageFormat)
{
    vk::AttachmentDescription colorAttachment{
        .format = swapchainImageFormat,
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

    return device.createRenderPass(renderPassInfo);
}

std::vector<vk::raii::Framebuffer> createFramebuffers(
    const vk::raii::Device &device,
    const std::vector<vk::raii::ImageView> &swapchainImageViews,
    const vk::raii::RenderPass &renderPass,
    const vk::Extent2D &swapchainExtent)
{
    std::vector<vk::raii::Framebuffer> buffers;
    buffers.reserve(swapchainImageViews.size());
    for (auto &&imageView : swapchainImageViews)
    {
        std::array attachments{
            *imageView,
        };

        vk::FramebufferCreateInfo frameBufferInfo{
            .sType = vk::StructureType::eFramebufferCreateInfo,
            .renderPass = *renderPass,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
            .layers = 1,
        };

        buffers.push_back(device.createFramebuffer(frameBufferInfo));
    }
    return buffers;
}

vk::raii::CommandPool createCommandPool(
    const vk::raii::Device &device,
    const QueueFamilyIndices &queueFamilyIndices)
{
    vk::CommandPoolCreateInfo poolInfo{
        .sType = vk::StructureType::eCommandPoolCreateInfo,
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
    };

    return device.createCommandPool(poolInfo);
}

vk::raii::CommandBuffers createCommandBuffers(const vk::raii::Device &device, const vk::raii::CommandPool &commandPool)
{
    vk::CommandBufferAllocateInfo allocateInfo{
        .sType = vk::StructureType::eCommandBufferAllocateInfo,
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };

    // TODO could be vector device.alloca... returns vector ?
    return vk::raii::CommandBuffers(device, allocateInfo);
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
    instance = createInstance(context);
    debugMessenger = createDebugMessenger(instance);
    surface = createSurface(instance, window);
    physicalDevice = pickPhysicalDevice(instance, surface);

    /*auto*/ indices = findQueueFamilies(physicalDevice, surface);

    device = createLogicalDevice(physicalDevice, indices);

    graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
    presentQueue = device.getQueue(indices.presentFamily.value(), 0);

    auto swapChainSupport = querySwapChainSupport(physicalDevice, surface);
    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    swapchainExtent = chooseSwapExtent(window, swapChainSupport.capabilities);

    swapchain = createSwapChain(device, surface, surfaceFormat, swapchainExtent, swapChainSupport, indices);

    swapChainImageViews = createImageViews(device, swapchain.getImages(), surfaceFormat.format);
    renderPass = createRenderPass(device, surfaceFormat.format);
    graphicsPipeline = createGraphicsPipeline(device, renderPass);
    swapChainFramebuffers = createFramebuffers(device, swapChainImageViews, renderPass, swapchainExtent);
    commandPool = createCommandPool(device, indices);
    commandBuffers = createCommandBuffers(device, commandPool);

    initSyncObjects();
}

void Application::mainLoop()
{
    uint32_t currentFrame = 0;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        drawFrame(currentFrame);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    // device.waitIdle();
}

void Application::cleanup()
{
    glfwDestroyWindow(window);

    glfwTerminate();
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
            .extent = swapchainExtent,
        },
        .clearValueCount = 1,
        .pClearValues = &clearColor,
    };

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

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRenderPass();

    commandBuffer.end();
}

void Application::drawFrame(uint32_t currentFrame)
{
    auto &imageAvailableSemaphore = imageAvailableSemaphores[currentFrame];
    auto &renderFinishedSemaphore = renderFinishedSemaphores[currentFrame];
    auto &inFlightFence = inFlightFences[currentFrame];
    auto &commandBuffer = commandBuffers[currentFrame];

    if (device.waitForFences(*inFlightFence, VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
    {
        std::cerr << "DrawFrame:\tCould not wait for fences\n";
    }

    auto [result, imageIndex] = swapchain.acquireNextImage(UINT64_MAX, *imageAvailableSemaphore);

    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        recreateSwapchain();
        return;
    }
    else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    device.resetFences(*inFlightFence);

    commandBuffer.reset();
    recordCommandBuffer(commandBuffer, imageIndex);

    std::array waitSephamores{*imageAvailableSemaphore};
    std::array<vk::PipelineStageFlags, waitSephamores.size()> waitStages{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    // TODO: assert that their sizes are the same
    std::array signalSephamores{
        *renderFinishedSemaphore,
    };

    vk::SubmitInfo submitInfo{
        .sType = vk::StructureType::eSubmitInfo,
        .waitSemaphoreCount = waitSephamores.size(),
        .pWaitSemaphores = waitSephamores.data(),
        .pWaitDstStageMask = waitStages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &*commandBuffer,
        .signalSemaphoreCount = signalSephamores.size(),
        .pSignalSemaphores = signalSephamores.data(),
    };

    graphicsQueue.submit(submitInfo, *inFlightFence);

    std::array swapchains = {*swapchain};
    vk::PresentInfoKHR presentInfo{
        .sType = vk::StructureType::ePresentInfoKHR,
        .waitSemaphoreCount = signalSephamores.size(),
        .pWaitSemaphores = signalSephamores.data(),
        .swapchainCount = swapchains.size(),
        .pSwapchains = swapchains.data(),
        .pImageIndices = &imageIndex};

    result = presentQueue.presentKHR(presentInfo);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        recreateSwapchain();
    }
    else if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }
}

void Application::initSyncObjects()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        imageAvailableSemaphores.push_back(device.createSemaphore({
            .sType = vk::StructureType::eSemaphoreCreateInfo,
        }));
        renderFinishedSemaphores.push_back(device.createSemaphore({
            .sType = vk::StructureType::eSemaphoreCreateInfo,
        }));

        inFlightFences.push_back(device.createFence({
            .sType = vk::StructureType::eFenceCreateInfo,
            .flags = vk::FenceCreateFlagBits::eSignaled,
        }));
    }
}

void Application::recreateSwapchain()
{
    device.waitIdle();

    //    swapchain.clear();
    //    for (auto &&views : swapChainImageViews)
    //    {
    //     views.clear();
    //    }
    //    for (auto &&buffers : swapChainFramebuffers)
    //    {
    //     buffers.clear();
    //    }

    auto swapChainSupport = querySwapChainSupport(physicalDevice, surface);

    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    swapchainExtent = chooseSwapExtent(window, swapChainSupport.capabilities);

    //  std::clog << swapchainExtent.width << " " << swapchainExtent.height << std::endl;

    swapchain = createSwapChain(device, surface, surfaceFormat, swapchainExtent, swapChainSupport, indices);

    swapChainImageViews = createImageViews(device, swapchain.getImages(), surfaceFormat.format);
    swapChainFramebuffers = createFramebuffers(device, swapChainImageViews, renderPass, swapchainExtent);
}