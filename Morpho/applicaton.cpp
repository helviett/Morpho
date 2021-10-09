#include "Application.hpp"

void Application::run() {
    init_window();
    main_loop();
}

void Application::init_window() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
}

void Application::main_loop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

void Application::set_graphics_context(Morpho::Vulkan::Context* context) {
    this->context = context;
}
