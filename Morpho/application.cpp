#include "application.hpp"
#include <fstream>
#include <filesystem>

void Application::run() {
    init_window();
    context->init(window);
    context->set_frame_context_count(2);

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

    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
    };
    const std::vector<uint16_t> indices = {
        0, 1, 2,
        2, 3, 0,
    };
    pipeline_info.add_vertex_binding_description(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    pipeline_info.add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position));
    pipeline_info.add_vertex_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color));
    auto vertex_buffer_size = (VkDeviceSize)(sizeof(vertices[0]) * vertices.size());
    auto staging_vertex_buffer = context->acquire_staging_buffer(
        vertex_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    staging_vertex_buffer.update(vertices.data(), vertex_buffer_size);
    vertex_buffer = context->acquire_buffer(
        vertex_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto index_buffer_size = (uint32_t)(sizeof(indices[0]) * indices.size());
    auto staging_index_buffer = context->acquire_staging_buffer(
        index_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    staging_index_buffer.update(indices.data(), index_buffer_size);
    index_buffer = context->acquire_buffer(
        vertex_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto cmd = context->acquire_command_buffer();
    cmd.copy_buffer(staging_vertex_buffer, vertex_buffer, vertex_buffer_size);
    cmd.copy_buffer(staging_index_buffer, index_buffer, index_buffer_size);
    context->flush(cmd);

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
    cmd.bind_vertex_buffer(vertex_buffer, 0);
    cmd.bind_index_buffer(index_buffer, VK_INDEX_TYPE_UINT16);
    cmd.draw_indexed(6, 1, 0, 0, 0);
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
