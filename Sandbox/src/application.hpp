#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vulkan/pipeline_state.hpp"
#include "vulkan/context.hpp"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>
#include "input/input.hpp"
#include "camera.hpp"

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 uv;
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
    Input input;
    GLFWwindow* window;
    Key key_map[Input::MAX_KEY_COUNT];
    Morpho::Vulkan::Context* context;
    Morpho::Vulkan::Buffer vertex_buffer;
    Morpho::Vulkan::Buffer index_buffer;
    Morpho::Vulkan::Shader vert_shader;
    Morpho::Vulkan::Shader frag_shader;
    Morpho::Vulkan::Image image;
    Morpho::Vulkan::ImageView image_view;
    Morpho::Vulkan::Sampler sampler;
    bool is_first_update = true;
    Camera camera;
    const float camera_sensetivity = 0.1f;
    glm::vec3 world_up = { 0, 1, 0 };
    glm::vec2 last_mouse_position = { 400, 300 };
    bool is_mouse_pressed = false;

    void main_loop();
    void initialize_static_resources(Morpho::Vulkan::CommandBuffer& cmd);
    void init_window();
    void cleanup();
    void render_frame();
    void initialize_key_map();
    void update(float delta);
    Key glfw_key_code_to_key(int code);
    static void process_keyboard_input(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void process_cursor_position(GLFWwindow* window, double xpos, double ypos);
    static void process_mouse_button_input(GLFWwindow* window, int button, int action, int mods);
    std::vector<char> read_file(const std::string& filename);
};
