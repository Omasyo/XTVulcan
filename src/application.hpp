#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <optional>

const u_int32_t WIDTH = 800;
const u_int32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

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

    void recordCommandBuffer(const vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);

    void recreateSwapchain();

    void drawFrame(uint32_t currentFrame);

    void initSyncObjects();

    GLFWwindow *window;

    vk::raii::Context context;
    vk::raii::Instance instance{nullptr};

    vk::raii::DebugUtilsMessengerEXT debugMessenger{nullptr};

    vk::raii::PhysicalDevice physicalDevice{nullptr};
    vk::raii::Device device{nullptr};

    vk::raii::Queue graphicsQueue{nullptr};
    vk::raii::SurfaceKHR surface{nullptr};
    VkSurfaceKHR tSurface{};

    vk::raii::Queue presentQueue{nullptr};

    vk::raii::SwapchainKHR swapchain{nullptr};
    // vk::Format swapchainImageFormat;
    // vk::SurfaceFormatKHR surfaceFormat;
    // SwapChainSupportDetails swapChainSupport;
    vk::Extent2D swapchainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    QueueFamilyIndices indices;

    vk::raii::RenderPass renderPass{nullptr};

    vk::raii::Pipeline graphicsPipeline{nullptr};
    std::vector<vk::raii::Framebuffer> swapChainFramebuffers;

    vk::raii::CommandPool commandPool{nullptr};
    vk::raii::CommandBuffers commandBuffers{nullptr};

    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore>  renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;

    // uint32_t currentFrame = 0;
};