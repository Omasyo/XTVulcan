// #include <vulkan/vulkan_raii.hpp>

#include <vulkan/vulkan_core.h> //TODO: Not sure why this isn't added in raii, since it looks like it's actually there

#include <GLFW/glfw3.h>

#include <iostream>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <map>
#include <set>
#include <cstdint>
#include <limits>

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
        .messageSeverity = Severity::eVerbose | Severity::eWarning | Severity::eWarning, // | Severity::eInfo,
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
}

void Application::mainLoop()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
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

    swapChainImages = vk::raii::SwapchainKHR(device, swapChain);

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}