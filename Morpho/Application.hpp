#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Application {
public:
    void run();
private:
    GLFWwindow* window;

    void main_loop();
    void init_window();
    void cleanup();
};
