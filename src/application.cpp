// #include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <map>

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

    createInfo.setMessageSeverity(Severity::eVerbose | Severity::eWarning | Severity::eWarning | Severity::eInfo)
        .setMessageType(MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance)
        .setPfnUserCallback(debugCallback);
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
    pickPhysicalDevice();
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

    vk::ApplicationInfo appInfo{.pApplicationName = "XTVulcan",
                                .applicationVersion = 1,
                                .pEngineName = "No Engine",
                                .engineVersion = 1,
                                .apiVersion = VK_API_VERSION_1_1};

    vk::InstanceCreateInfo createInfo{
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

QueueFamilyIndices findQueueFamilies(vk::raii::PhysicalDevice device)
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

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

bool isDeviceSuitable(vk::raii::PhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    return indices.isComplete();
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