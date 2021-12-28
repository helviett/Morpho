#include "application.hpp"

void Application::run() {
    init_window();
    context->init(window);
    context->set_frame_context_count(2);
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
        render_frame();
    }
}

void Application::set_graphics_context(Morpho::Vulkan::Context* context) {
    this->context = context;
}

void Application::render_frame() {
    context->begin_frame();
    auto cmd = context->acquire_command_buffer();

    Morpho::Vulkan::RenderPassInfo info;
    info.clear_value = { 1.0f, 0.0f, 0.0f, };
    info.image_view = context->get_swapchain_image_view();
    context->begin_render_pass(cmd, info);
    context->end_render_pass(cmd);

    context->submit(cmd);
    context->end_frame();
}
