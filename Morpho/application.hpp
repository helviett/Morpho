#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vulkan/pipeline_info.hpp"
#include "vulkan/context.hpp"


class Application {
public:
    void run();
    void set_graphics_context(Morpho::Vulkan::Context* context);
private:
    GLFWwindow* window;
    Morpho::Vulkan::Context* context;
    Morpho::Vulkan::PipelineInfo pipeline_info;


    void main_loop();
    void init_window();
    void cleanup();
    void render_frame();
    std::vector<char> read_file(const std::string& filename);
};
