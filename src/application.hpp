#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <ranges>
#include <string>
#include <unordered_set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const char *APP_NAME = "Hello Triangle";

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
    vk::raii::PipelineLayout pipelineLayout{nullptr};
    vk::raii::Pipeline graphicsPipeline{nullptr};

    std::vector<vk::raii::Framebuffer> swapchainFrameBuffers;

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

        createGraphicsPipeline();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
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

        // Find descrete GPU that supports required device extensions
        for (const auto &device : physicalDevices)
        {
            auto properties = device.enumerateDeviceExtensionProperties();
            if (
                (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) &&
                (checkExtensionSupport(deviceExtensions, properties)))
            {
                physicalDevice = device;
                return;
            }
        }

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

        queueFamilyIndices = getQueueFamilyIndices(properties, vk::QueueFlagBits::eGraphics);
        // queueFamilyIndices.presentFamily = getQueueFamilyIndex(properties, vk::QueueFlagBits::eTransfer);

        float priority = 1.0f;
        std::array<vk::DeviceQueueCreateInfo, 2> deviceQueueCreateInfos{
            vk::DeviceQueueCreateInfo{
                .queueFamilyIndex = queueFamilyIndices.graphicsFamily,
                .queueCount = 1,
                .pQueuePriorities = &priority,
            },
            vk::DeviceQueueCreateInfo{
                .queueFamilyIndex = queueFamilyIndices.presentFamily,
                .queueCount = 1,
                .pQueuePriorities = &priority,
            }};

        vk::PhysicalDeviceFeatures deviceFeatures{};

        vk::DeviceCreateInfo deviceCreateInfo{
            .queueCreateInfoCount = deviceQueueCreateInfos.size(),
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
        const std::vector<vk::QueueFamilyProperties> &properties,
        vk::QueueFlags queueFlags)
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
            // .oldSwapchain = nullptr
        };

        std::array indices = {queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily};

        if (queueFamilyIndices.graphicsFamily == queueFamilyIndices.presentFamily)
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

        renderPass = logicalDevice.createRenderPass({
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
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

        vk::PipelineVertexInputStateCreateInfo vertexInputStateInfo{

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
            .frontFace = vk::FrontFace::eClockwise,
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
        swapchainFrameBuffers.reserve(swapchainImageViews.size());

        for (auto &&imageViews : swapchainImageViews)
        {
            std::array attachments{*imageViews};

            swapchainFrameBuffers.push_back(
                logicalDevice.createFramebuffer({
                    .renderPass = *renderPass,
                    .attachmentCount = static_cast<uint32_t>(attachments.size()),
                    .pAttachments = attachments.data(),
                    .width = swapchainExtent.width,
                    .height = swapchainExtent.height,
                    .layers = 1
                })
            );
        }
    }
};