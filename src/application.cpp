// #include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <algorithm>
#include <cstring>

#include "application.hpp"

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

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.setEnabledExtensionCount(glfwExtensionCount)
        .setPpEnabledExtensionNames(glfwExtensions)

        if(enableValidationLayers) {
            createInfo.setEnabledLayerCount(validationLayers.size()) 
            .setPpEnabledLayerNames(validationLayers.data());
        }

    instance = vk::raii::Instance(context, createInfo);

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
                            return strcmp(layer, properties.layerName) == 0; 
                            }) == availableLayers.cend())
        {
        std::cout << '\t' << layer << " not found" << std::endl;
            return false;
        }
    }

    return true;
}