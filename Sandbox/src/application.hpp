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
#include <tiny_gltf.h>
#include <filesystem>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
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
    bool load_scene(std::filesystem::path file_path);
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
    Morpho::Vulkan::Sampler default_sampler;
    Morpho::Vulkan::Image white_image;
    Morpho::Vulkan::ImageView white_image_view;
    std::vector<Morpho::Vulkan::Buffer> buffers;
    std::vector<Morpho::Vulkan::Image> texture_images;
    std::vector<Morpho::Vulkan::ImageView> texture_image_views;
    std::vector<Morpho::Vulkan::Sampler> samplers;

    bool is_first_update = true;
    Camera camera;
    const float camera_sensetivity = 0.1f;
    glm::vec3 world_up = { 0, 1, 0 };
    glm::vec2 last_mouse_position = { 400, 300 };
    bool is_mouse_pressed = false;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    int current_material_index = -1;
    // Perhaps should be retrieved via reflection.
    std::map<std::string, uint32_t> attribute_name_to_location = {
        {"POSITION", 0},
        {"NORMAL", 1},
        {"TEXCOORD_0", 2},
    };

    void main_loop();
    void initialize_static_resources(Morpho::Vulkan::CommandBuffer& cmd);
    void init_window();
    void cleanup();
    void render_frame();
    void initialize_key_map();
    void update(float delta);
    Key glfw_key_code_to_key(int code);
    void create_scene_resources(Morpho::Vulkan::CommandBuffer& cmd);
    void draw_model(const tinygltf::Model& model, Morpho::Vulkan::CommandBuffer& cmd);
    void draw_scene(const tinygltf::Model& model, const tinygltf::Scene& scene, Morpho::Vulkan::CommandBuffer& cmd);
    void draw_node(
        const tinygltf::Model& model,
        const tinygltf::Node& node,
        Morpho::Vulkan::CommandBuffer& cmd,
        glm::mat4 parent_to_world
    );
    void draw_mesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, Morpho::Vulkan::CommandBuffer& cmd);
    void draw_primitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, Morpho::Vulkan::CommandBuffer& cmd);
    static void process_keyboard_input(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void process_cursor_position(GLFWwindow* window, double xpos, double ypos);
    static void process_mouse_button_input(GLFWwindow* window, int button, int action, int mods);
    static VkFormat gltf_to_format(int type, int component_type);
    static VkIndexType gltf_to_index_type(int type, int component_type);
    static VkFilter gltf_to_filter(int filter);
    static VkSamplerAddressMode gltf_to_sampler_address_mode(int address_mode);
    std::vector<char> read_file(const std::string& filename);
};
