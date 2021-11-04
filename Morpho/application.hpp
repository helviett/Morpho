#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vulkan/context.hpp"


class Application {
public:
    void run();
    void set_graphics_context(Morpho::Vulkan::Context* context);
private:
    GLFWwindow* window;
    Morpho::Vulkan::Context* context;

    void main_loop();
    void init_window();
    void cleanup();
};
