#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vulkan/pipeline_state.hpp"
#include "vulkan/context.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex {
    glm::vec2 position;
    glm::vec3 color;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class Application {
public:
    void run();
    void set_graphics_context(Morpho::Vulkan::Context* context);
private:
    GLFWwindow* window;
    Morpho::Vulkan::Context* context;
    Morpho::Vulkan::Buffer vertex_buffer;
    Morpho::Vulkan::Buffer index_buffer;
    Morpho::Vulkan::Shader vert_shader;
    Morpho::Vulkan::Shader frag_shader;

    void main_loop();
    void init_window();
    void cleanup();
    void render_frame();
    std::vector<char> read_file(const std::string& filename);
};
