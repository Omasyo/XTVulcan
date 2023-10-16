#include <vulkan/vulkan_core.h> //TODO: Not sure why this isn't added in raii, since it looks like it's actually there

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
#include <chrono>

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

vk::raii::Pipeline createGraphicsPipeline(
    const vk::raii::Device &device,
    const vk::raii::RenderPass &renderPass,
    vk::raii::PipelineLayout &pipelineLayout,
    const vk::raii::DescriptorSetLayout &descriptorSetLayout)
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

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = vk::StructureType::ePipelineVertexInputStateCreateInfo,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = attributeDescriptions.size(),
        .pVertexAttributeDescriptions = attributeDescriptions.data(),
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
        .frontFace = vk::FrontFace::eCounterClockwise,
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
        .setLayoutCount = 1,
        .pSetLayouts = &*descriptorSetLayout};

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

// vk::raii::Buffer createVertexBuffer(const vk::raii::Device &device)
// {
//     vk::BufferCreateInfo createInfo{
//         .sType = vk::StructureType::eBufferCreateInfo,
//         .size = sizeof(vertices[0]) * vertices.size(),
//         .usage = vk::BufferUsageFlagBits::eVertexBuffer,
//         .sharingMode = vk::SharingMode::eExclusive};

//     return device.createBuffer(createInfo);
// }

uint32_t findMemoryType(
    const vk::raii::PhysicalDevice physicalDevice,
    uint32_t typeFilter,
    vk::MemoryPropertyFlags properties)
{
    auto memoryProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

vk::raii::DeviceMemory allocateBufferMemory(
    const vk::raii::Device &device,
    const vk::raii::PhysicalDevice &physicalDevice,
    const vk::MemoryRequirements &memoryRequirements,
    vk::MemoryPropertyFlags properties)
{
    vk::MemoryAllocateInfo allocInfo{
        .sType = vk::StructureType::eMemoryAllocateInfo,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = findMemoryType(
            physicalDevice,
            memoryRequirements.memoryTypeBits,
            properties),
    };
    return device.allocateMemory(allocInfo);
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

    return vk::raii::CommandBuffers(device, allocateInfo);
}

void copyBuffer(
    const vk::raii::Device &device,
    const vk::raii::CommandPool &commandPool,
    const vk::raii::Queue queue,
    const vk::raii::Buffer &source,
    const vk::raii::Buffer &destination,
    vk::DeviceSize size)
{

    auto commandBuffers = device.allocateCommandBuffers({
        .sType = vk::StructureType::eCommandBufferAllocateInfo,
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });

    const auto &commandBuffer = commandBuffers[0];

    commandBuffer.begin({
        .sType = vk::StructureType::eCommandBufferBeginInfo,
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    std::array copyRegions{vk::BufferCopy{.size = size}};

    commandBuffer.copyBuffer(*source, *destination, copyRegions);
    commandBuffer.end();

    vk::SubmitInfo info{
        .sType = vk::StructureType::eSubmitInfo,
        .commandBufferCount = commandBuffers.size(),
        .pCommandBuffers = &*commandBuffer,
    };
    queue.submit({info});
    queue.waitIdle();
}

vk::raii::DescriptorSetLayout createDescriptorSetLayout(const vk::raii::Device &device)
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
    };

    vk::DescriptorSetLayoutCreateInfo createInfo{
        .sType = vk::StructureType::eDescriptorSetLayoutCreateInfo,
        .bindingCount = 1,
        .pBindings = &uboLayoutBinding,
    };
    return device.createDescriptorSetLayout(createInfo);
}

vk::raii::DescriptorPool createDescriptionPool(const vk::raii::Device &device)
{
    vk::DescriptorPoolSize poolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
    };

    vk::DescriptorPoolCreateInfo poolInfo{
        .sType = vk::StructureType::eDescriptorPoolCreateInfo,
        .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };

    return device.createDescriptorPool(poolInfo);
}

vk::raii::DescriptorSets createDescriptorSets(
    const vk::raii::Device &device,
    const vk::raii::DescriptorPool &descriptorPool,
    const vk::raii::DescriptorSetLayout &descriptorSetLayout)
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{
        .sType = vk::StructureType::eDescriptorSetAllocateInfo,
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layouts.data(),
    };

    return vk::raii::DescriptorSets(device, allocInfo);
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

    /*auto*/ familyIndices = findQueueFamilies(physicalDevice, surface);

    device = createLogicalDevice(physicalDevice, familyIndices);

    graphicsQueue = device.getQueue(familyIndices.graphicsFamily.value(), 0);
    presentQueue = device.getQueue(familyIndices.presentFamily.value(), 0);

    auto swapChainSupport = querySwapChainSupport(physicalDevice, surface);
    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    swapchainExtent = chooseSwapExtent(window, swapChainSupport.capabilities);

    swapchain = createSwapChain(device, surface, surfaceFormat, swapchainExtent, swapChainSupport, familyIndices);

    swapChainImageViews = createImageViews(device, swapchain.getImages(), surfaceFormat.format);
    renderPass = createRenderPass(device, surfaceFormat.format);

    descriptorSetLayout = createDescriptorSetLayout(device);

    graphicsPipeline = createGraphicsPipeline(device, renderPass, pipelineLayout ,descriptorSetLayout);
    swapChainFramebuffers = createFramebuffers(device, swapChainImageViews, renderPass, swapchainExtent);

    commandPool = createCommandPool(device, familyIndices);

    auto stagingBuffer = device.createBuffer({
        .sType = vk::StructureType::eBufferCreateInfo,
        .size = sizeof(vertices[0]) * vertices.size(),
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive,
    });
    auto memoryRequirements = stagingBuffer.getMemoryRequirements();
    auto stagingBufferMemory = allocateBufferMemory(
        device,
        physicalDevice,
        memoryRequirements,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    stagingBuffer.bindMemory(*stagingBufferMemory, 0);

    auto data = stagingBufferMemory.mapMemory(0, memoryRequirements.size);
    memcpy(data, vertices.data(), memoryRequirements.size);
    stagingBufferMemory.unmapMemory();

    vertexBuffer = device.createBuffer({
        .sType = vk::StructureType::eBufferCreateInfo,
        .size = sizeof(vertices[0]) * vertices.size(),
        .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
    });

    memoryRequirements = vertexBuffer.getMemoryRequirements();

    vertexBufferMemory = allocateBufferMemory(
        device,
        physicalDevice,
        memoryRequirements,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    vertexBuffer.bindMemory(*vertexBufferMemory, 0);

    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, memoryRequirements.size);

    stagingBuffer = device.createBuffer({
        .sType = vk::StructureType::eBufferCreateInfo,
        .size = sizeof(indices[0]) * indices.size(),
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    });
    memoryRequirements = stagingBuffer.getMemoryRequirements();
    stagingBufferMemory = allocateBufferMemory(
        device,
        physicalDevice,
        memoryRequirements,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    stagingBuffer.bindMemory(*stagingBufferMemory, 0);

    data = stagingBufferMemory.mapMemory(0, memoryRequirements.size);
    memcpy(data, indices.data(), memoryRequirements.size);
    stagingBufferMemory.unmapMemory();

    indexBuffer = device.createBuffer({

        .sType = vk::StructureType::eBufferCreateInfo,
        .size = sizeof(indices[0]) * indices.size(),
        .usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
    });
    memoryRequirements = vertexBuffer.getMemoryRequirements();

    indexBufferMemory = allocateBufferMemory(
        device,
        physicalDevice,
        memoryRequirements,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    indexBuffer.bindMemory(*indexBufferMemory, 0);

    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, indexBuffer, memoryRequirements.size);

    uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.reserve(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        auto buffer = device.createBuffer({
            .sType = vk::StructureType::eBufferCreateInfo,
            .size = sizeof(UniformBufferObject),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
        });
        memoryRequirements = buffer.getMemoryRequirements();
        auto bufferMemory = allocateBufferMemory(
            device,
            physicalDevice,
            memoryRequirements,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        buffer.bindMemory(*bufferMemory, 0);
        data = bufferMemory.mapMemory(0, memoryRequirements.size);

        uniformBuffers.push_back(std::move(buffer));
        uniformBuffersMemory.push_back(std::move(bufferMemory));
        uniformBuffersMapped.push_back(std::move(data));
    }

    descriptionPool = createDescriptionPool(device);
    descriptorSets = createDescriptorSets(device, descriptionPool, descriptorSetLayout);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = *uniformBuffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject),
        };

        vk::WriteDescriptorSet descriptorWrite{
            .sType = vk::StructureType::eWriteDescriptorSet,
            .dstSet = *descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &bufferInfo,
        };

        device.updateDescriptorSets(descriptorWrite, nullptr);
    }

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

void Application::recordCommandBuffer(
    const vk::raii::CommandBuffer &commandBuffer, 
    uint32_t imageIndex, uint32_t currentFrame)
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

    std::array vertexBuffers{*vertexBuffer};
    std::array<vk::DeviceSize, 1> offsets{0};

    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);

    commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, 
        *pipelineLayout,0, {*descriptorSets[currentFrame]}, nullptr);
    commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    // commandBuffer.draw(3, 1, 0, 0);

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

    updateUniformBuffer(currentFrame);

    device.resetFences(*inFlightFence);

    commandBuffer.reset();
    recordCommandBuffer(commandBuffer, imageIndex, currentFrame);

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

    swapchain = createSwapChain(device, surface, surfaceFormat, swapchainExtent, swapChainSupport, familyIndices);

    swapChainImageViews = createImageViews(device, swapchain.getImages(), surfaceFormat.format);
    swapChainFramebuffers = createFramebuffers(device, swapChainImageViews, renderPass, swapchainExtent);
}

void Application::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.proj = glm::perspective(glm::radians(45.0f), swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.0f);

    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}