#include "application.hpp"
#include <fstream>
#include <filesystem>

void Application::run() {
    init_window();
    context->init(window);
    context->set_frame_context_count(2);
    std::cout << std::filesystem::current_path() << std::endl;
    auto vert_code = read_file("./assets/shaders/triangle.vert.spv");
    auto frag_code = read_file("./assets/shaders/triangle.frag.spv");
    auto vert_shader = context->acquire_shader(vert_code.data(), (uint32_t)vert_code.size());
    vert_shader.set_stage(Morpho::Vulkan::ShaderStage::Vertex);
    vert_shader.set_entry_point("main");
    auto frag_shader = context->acquire_shader(frag_code.data(), (uint32_t)frag_code.size());
    frag_shader.set_stage(Morpho::Vulkan::ShaderStage::Fragment);
    frag_shader.set_entry_point("main");
    pipeline_info.add_shader(vert_shader);
    pipeline_info.add_shader(frag_shader);
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
    info.clear_value = { 0.0f, 0.0f, 0.0f, };
    info.image_view = context->get_swapchain_image_view();
    cmd.begin_render_pass(info);
    cmd.bind_pipeline(pipeline_info);
    cmd.draw(3, 1, 0, 0);
    cmd.end_render_pass();

    context->submit(cmd);
    context->end_frame();
}

std::vector<char> Application::read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + filename);
    }
    auto file_size = (size_t)file.tellg();
    std::vector<char> data(file_size);
    file.seekg(0);
    file.read(data.data(), file_size);
    file.close();

    return data;
}
