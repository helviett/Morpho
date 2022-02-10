#include "application.hpp"
#include <fstream>
#include <filesystem>

void Application::run() {
    init_window();
    context->init(window);
    context->set_frame_context_count(2);

    auto vert_code = read_file("./assets/shaders/triangle.vert.spv");
    auto frag_code = read_file("./assets/shaders/triangle.frag.spv");
    vert_shader = context->acquire_shader(vert_code.data(), (uint32_t)vert_code.size());
    vert_shader.set_stage(Morpho::Vulkan::ShaderStage::Vertex);
    vert_shader.set_entry_point("main");
    frag_shader = context->acquire_shader(frag_code.data(), (uint32_t)frag_code.size());
    frag_shader.set_stage(Morpho::Vulkan::ShaderStage::Fragment);
    frag_shader.set_entry_point("main");


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
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    context->begin_frame();
    auto cmd = context->acquire_command_buffer();
    cmd.add_shader(vert_shader);
    cmd.add_shader(frag_shader);
    cmd.add_vertex_binding_description(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    cmd.add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position));
    cmd.add_vertex_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color));


    Morpho::Vulkan::RenderPassInfo render_pass;
    render_pass.clear_value = { 0.0f, 0.0f, 0.0f, };
    render_pass.image_view = context->get_swapchain_image_view();
    cmd.begin_render_pass(render_pass);
    auto uniform1 = context->acquire_staging_buffer(
        sizeof(UniformBufferObject),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    auto uniform2 = context->acquire_staging_buffer(
        sizeof(UniformBufferObject),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    void* data;
    context->map_memory(uniform1.get_allocation(), &data);
    auto swapchain_extent = context->get_swapchain_extent();
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swapchain_extent.width / (float) swapchain_extent.height, 0.1f, 10.0f);
    memcpy(data, &ubo, sizeof(ubo));
    context->unmap_memory(uniform1.get_allocation());
    ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f))
        * glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    context->map_memory(uniform2.get_allocation(), &data);
    memcpy(data, &ubo, sizeof(ubo));
    context->unmap_memory(uniform2.get_allocation());
    cmd.set_uniform_buffer(1, 0, uniform1, 0, sizeof(UniformBufferObject));
    cmd.bind_vertex_buffer(vertex_buffer, 0);
    cmd.bind_index_buffer(index_buffer, VK_INDEX_TYPE_UINT16);
    cmd.draw_indexed(6, 1, 0, 0, 0);
    cmd.set_uniform_buffer(1, 0, uniform2, 0, sizeof(UniformBufferObject));
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
