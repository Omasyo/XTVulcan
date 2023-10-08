#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <optional>

const u_int32_t WIDTH = 800;
const u_int32_t HEIGHT = 600;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class Application
{
public:
    void run();

private:
    void initWindow();

    void initVulkan();

    void mainLoop();

    void cleanup();

    void createInstance();

    bool checkValidationLayerSupport();

    std::vector<const char *> getRequiredExtensions();

    void setupDebugMessenger();

    void pickPhysicalDevice();

    bool isDeviceSuitable(vk::raii::PhysicalDevice device);

    bool checkDeviceExtensionSupport(vk::raii::PhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(vk::raii::PhysicalDevice device);

    void createLogicalDevice();

    void createSurface();

    SwapChainSupportDetails querySwapChainSupport(vk::raii::PhysicalDevice device);

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);

    void createSwapChain();

    GLFWwindow *window;

    vk::raii::Context context;
    vk::raii::Instance instance{nullptr};

    vk::raii::DebugUtilsMessengerEXT debugMessenger{nullptr};

    vk::raii::PhysicalDevice physicalDevice{nullptr};
    vk::PhysicalDeviceFeatures deviceFeatures;
    vk::raii::Device device{nullptr};

    vk::raii::Queue graphicsQueue{nullptr};
    vk::raii::SurfaceKHR surface{nullptr};
    VkSurfaceKHR tSurface{};

    vk::raii::Queue presentQueue{nullptr};

    vk::raii::SwapchainKHR swapChain{nullptr};
    std::vector<vk::Image> swapChainImages; //TODO : I don't need this
    vk::Format swapChainImageFormat;
    vk::Extent2D swapChainExtent;
};