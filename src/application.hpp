#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS

// #include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

class GLFWwindow; 

const u_int32_t WIDTH = 800;
const u_int32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

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

    GLFWwindow *window;

    vk::raii::Context context;

    vk::raii::Instance instance{nullptr};
};