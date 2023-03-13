#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vulkan/pipeline_state.hpp"
#include "vulkan/context.hpp"
// #define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>
#include "input/input.hpp"
#include "camera.hpp"
#include <tiny_gltf.h>
#include <filesystem>
#include <unordered_set>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct ViewProjection {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct DirectionalLight {
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 color;
};

struct PointLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
    alignas(4) float radius;
    alignas(4) float intensity;

    PointLight(glm::vec3 position, glm::vec3 color, float radius, float intensity)
        : position(position), color(color), radius(radius), intensity(intensity)
    {

    }

    void set_radius(float radius) {
        this->radius = radius;
    }

    void set_intensity(float intensity) {
        this->intensity = intensity;
    }
};

struct SpotLight {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 color;
    alignas(4) float radius;
    alignas(4) float intensity;
    alignas(4) float umbra;
    alignas(4) float penumbra;

    SpotLight() {

    }

    SpotLight(
        glm::vec3 position,
        glm::vec3 direction,
        glm::vec3 color,
        float radius,
        float intensity,
        float umbra,
        float penumbra
    ) : position(position), direction(direction), color(color),
        radius(radius), intensity(intensity),
        umbra(umbra), penumbra(penumbra)
    {
    }
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
    Morpho::Vulkan::Shader gltf_depth_pass_vertex_shader;
    Morpho::Vulkan::Shader gltf_spot_light_vertex_shader;
    Morpho::Vulkan::Shader gltf_point_light_vertex_shader;
    Morpho::Vulkan::Shader gltf_spot_light_fragment_shader;
    Morpho::Vulkan::Shader gltf_point_light_fragment_shader;
    Morpho::Vulkan::Shader no_light_vertex_shader;
    Morpho::Vulkan::Shader no_light_fragment_shader;
    Morpho::Vulkan::Shader depth_vertex_shader;
    Morpho::Vulkan::Shader shadow_map_spot_light_fragment_shader;
    Morpho::Vulkan::Image image;
    Morpho::Vulkan::ImageView image_view;
    Morpho::Vulkan::Sampler default_sampler;
    Morpho::Vulkan::Sampler shadow_sampler;
    Morpho::Vulkan::Image white_image;
    Morpho::Vulkan::ImageView white_image_view;
    std::vector<Morpho::Vulkan::Buffer> buffers;
    std::vector<Morpho::Vulkan::Image> texture_images;
    std::vector<Morpho::Vulkan::ImageView> texture_image_views;
    std::vector<Morpho::Vulkan::Sampler> samplers;
    std::vector<Morpho::Vulkan::ImageView> shadow_maps;
    uint32_t frames = 0;
    // DirectionalLight directional_light = {
    //     glm::vec3(0.0f, -1.0f, 0.0f),
    //     glm::vec3(1.0f, 1.0f, 1.0f),
    // };
    // PointLight point_light = PointLight(glm::vec3(0.0f), glm::vec3(1.0f, 0.0, 0.0), 10.0f, 10.0f);
    std::vector<SpotLight> spot_lights;
    std::vector<PointLight> point_lights;

    bool is_first_update = true;
    Camera camera;
    const float camera_sensetivity = 0.1f;
    glm::vec3 world_up = { 0.0f, 1.0f, 0.0f};
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
        {"TANGENT", 3},
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
    void draw_depth_image(Morpho::Vulkan::CommandBuffer &cmd, Morpho::Vulkan::ImageView depth_map);
    void draw_mesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, Morpho::Vulkan::CommandBuffer& cmd);
    void draw_primitive(
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        Morpho::Vulkan::CommandBuffer& cmd
    );
    Morpho::Vulkan::ImageView render_depth_pass(Morpho::Vulkan::CommandBuffer& cmd, const SpotLight& spot_light);
    Morpho::Vulkan::ImageView render_depth_pass(Morpho::Vulkan::CommandBuffer& cmd, const PointLight& point_light);
    void begin_color_pass(Morpho::Vulkan::CommandBuffer& cmd);
    void render_color_pass(
        Morpho::Vulkan::CommandBuffer& cmd,
        Morpho::Vulkan::ImageView shadow_map,
        const SpotLight& spot_light
    );
    void render_color_pass(
        Morpho::Vulkan::CommandBuffer& cmd,
        Morpho::Vulkan::ImageView shadow_map,
        const PointLight& point_light
    );
    void setup_spot_light_uniforms(Morpho::Vulkan::CommandBuffer& cmd, const SpotLight& spot_light);
    void setup_point_light_uniforms(Morpho::Vulkan::CommandBuffer& cmd, const PointLight& point_light);
    Morpho::Vulkan::Shader load_shader(const std::string& path);
    static void process_keyboard_input(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void process_cursor_position(GLFWwindow* window, double xpos, double ypos);
    static void process_mouse_button_input(GLFWwindow* window, int button, int action, int mods);
    static VkFormat gltf_to_format(int type, int component_type);
    static VkIndexType gltf_to_index_type(int type, int component_type);
    static VkFilter gltf_to_filter(int filter);
    static VkSamplerAddressMode gltf_to_sampler_address_mode(int address_mode);
    std::vector<char> read_file(const std::string& filename);
};
