#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <ranges>

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;
const char *APP_NAME = "Hello Triangle";

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

uint32_t getQueueFamilyIndex(const std::vector<vk::QueueFamilyProperties> &properties, vk::QueueFlags queueFlags)
{
    // Dedicated queue for compute
    // Try to find a queue family index that supports compute but not graphics
    if ((queueFlags & vk::QueueFlagBits::eCompute) == queueFlags)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(properties.size()); i++)
        {
            if ((properties[i].queueFlags & vk::QueueFlagBits::eCompute) && !(properties[i].queueFlags & vk::QueueFlagBits::eGraphics))
            {
                return i;
            }
        }
    }

    // Dedicated queue for transfer
    // Try to find a queue family index that supports transfer but not graphics and compute
    if ((queueFlags & vk::QueueFlagBits::eTransfer) == queueFlags)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(properties.size()); i++)
        {
            if ((properties[i].queueFlags & vk::QueueFlagBits::eTransfer) && !(properties[i].queueFlags & vk::QueueFlagBits::eGraphics) && !(properties[i].queueFlags & vk::QueueFlagBits::eCompute))
            {
                return i;
            }
        }
    }

    // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
    for (uint32_t i = 0; i < static_cast<uint32_t>(properties.size()); i++)
    {
        if ((properties[i].queueFlags & queueFlags) == queueFlags)
        {
            return i;
        }
    }

    throw std::runtime_error("Could not find a matching queue family index");
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

    struct
    {
        uint32_t graphicsFamily;
        uint32_t presentFamily;
    } queueFamilyIndices;

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
            createInfo.setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()))
                .setPpEnabledLayerNames(validationLayers.data());
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
        bool foundDevice = false;

        for (const auto &device : physicalDevices)
        {
            if (device.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                physicalDevice = device;
                foundDevice = true;
                break;
            }
        }

        if (!foundDevice)
        {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice()
    {
        auto properties = physicalDevice.getQueueFamilyProperties();

        queueFamilyIndices.graphicsFamily = getQueueFamilyIndex(properties, vk::QueueFlagBits::eGraphics);
        // queueFamilyIndices.presentFamily = getQueueFamilyIndex(properties, vk::QueueFlagBits::eTransfer);

        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = queueFamilyIndices.graphicsFamily,
            .queueCount = 1,
        };

        vk::PhysicalDeviceFeatures deviceFeatures{};

        vk::DeviceCreateInfo deviceCreateInfo{
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = 0,
            .pEnabledFeatures = &deviceFeatures,
        };

        if (enableValidationLayers)
        {
            deviceCreateInfo.setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()))
                .setPpEnabledLayerNames(validationLayers.data());
        }
        logicalDevice = physicalDevice.createDevice(deviceCreateInfo);
    }
};