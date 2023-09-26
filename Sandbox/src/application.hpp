#pragma once

#include "vulkan/resources.hpp"
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
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

struct MaterialParameters {
    // 16 bytes
    glm::vec4 base_color_factor;
    // 256 - 16 bytes
    uint32_t padding[60];
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

enum class LightType {
    SpotLight,
    PointLight,
};

struct Light {
    LightType light_type;
    uint32_t descriptor_set_start_index;
    Morpho::Vulkan::Texture shadow_map;
    Morpho::Vulkan::Texture views[6];
    union LightData {
        SpotLight spot_light;
        PointLight point_light;
    } light_data;
};

class Application {
public:
    void init();
    void run();
    void set_graphics_context(Morpho::Vulkan::Context* context);
    bool load_scene(std::filesystem::path file_path);
private:
    static const uint32_t frame_in_flight_count = 2;
    static const uint32_t max_light_count = 128;

    Input input;
    GLFWwindow* window;
    Key key_map[Input::MAX_KEY_COUNT];
    Morpho::Vulkan::Context* context;

    Morpho::Vulkan::RenderPassLayout color_pass_layout;
    Morpho::Vulkan::RenderPassLayout depth_pass_layout;
    Morpho::Vulkan::PipelineLayout light_pipeline_layout;
    Morpho::Vulkan::Pipeline depth_pass_pipeline_ccw;
    Morpho::Vulkan::Pipeline depth_pass_pipeline_ccw_double_sided;
    Morpho::Vulkan::Pipeline depth_pass_pipeline_cw;
    Morpho::Vulkan::Pipeline depth_pass_pipeline_cw_double_sided;
    Morpho::Vulkan::Pipeline spotlight_pipeline;
    Morpho::Vulkan::Pipeline spotlight_pipeline_double_sided;
    Morpho::Vulkan::Pipeline pointlight_pipeline;
    Morpho::Vulkan::Pipeline pointlight_pipeline_double_sided;
    Morpho::Vulkan::Pipeline no_light_pipeline;
    Morpho::Vulkan::Pipeline no_light_pipeline_double_sided;
    Morpho::Vulkan::Pipeline shadow_map_visualization_pipeline;
    Morpho::Vulkan::Pipeline z_prepass_pipeline;
    Morpho::Vulkan::Pipeline z_prepass_pipeline_double_sided;
    Morpho::Vulkan::RenderPass color_pass;
    Morpho::Vulkan::RenderPass depth_pass;
    Morpho::Vulkan::Shader z_prepass_shader;
    Morpho::Vulkan::Shader gltf_depth_pass_vertex_shader;
    Morpho::Vulkan::Shader gltf_spot_light_vertex_shader;
    Morpho::Vulkan::Shader gltf_point_light_vertex_shader;
    Morpho::Vulkan::Shader gltf_spot_light_fragment_shader;
    Morpho::Vulkan::Shader gltf_point_light_fragment_shader;
    Morpho::Vulkan::Shader no_light_vertex_shader;
    Morpho::Vulkan::Shader no_light_fragment_shader;
    Morpho::Vulkan::Shader full_screen_triangle_shader;
    Morpho::Vulkan::Shader shadow_map_spot_light_fragment_shader;
    Morpho::Vulkan::Sampler default_sampler;
    Morpho::Vulkan::Sampler shadow_sampler;
    Morpho::Vulkan::Texture white_texture;
    Morpho::Vulkan::Texture depth_buffer;
    std::vector<Morpho::Vulkan::Buffer> buffers;
    std::vector<Morpho::Vulkan::Texture> textures;
    std::vector<Morpho::Vulkan::Sampler> samplers;
    Morpho::Vulkan::Buffer globals_buffer;
    Morpho::Vulkan::DescriptorSet global_descriptor_sets[frame_in_flight_count];
    Morpho::Vulkan::Buffer material_buffer;
    std::vector<Morpho::Vulkan::DescriptorSet> material_descriptor_sets;
    Morpho::Vulkan::Buffer light_buffer;
    std::vector<Morpho::Vulkan::DescriptorSet> light_descriptor_sets;
    Morpho::Vulkan::DescriptorSet shadow_map_visualization_descriptor_set[frame_in_flight_count];
    Morpho::Vulkan::Buffer mesh_uniforms;
    std::vector<Morpho::Vulkan::DescriptorSet> mesh_descriptor_sets;
    // TODO: temp solution just to keep light code consistent.
    Morpho::Vulkan::Buffer cube_map_face_buffer;
    std::vector<Morpho::Vulkan::DescriptorSet> cube_map_face_descriptor_sets;
    uint32_t frames_total = 0;
    uint32_t frame_index = 0;
    std::vector<Light> lights;

    bool is_first_update = true;
    Camera camera;
    const float camera_sensetivity = 0.1f;
    glm::vec3 world_up = { 0.0f, 1.0f, 0.0f};
    glm::vec2 last_mouse_position = { 400, 300 };
    bool is_mouse_pressed = false;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    int current_material_index = -1;
    Morpho::Vulkan::Pipeline currently_bound_pipeline = {};
    // Perhaps should be retrieved via reflection.
    std::map<std::string, uint32_t> attribute_name_to_location = {
        {"POSITION", 0},
        {"NORMAL", 1},
        {"TEXCOORD_0", 2},
        {"TANGENT", 3},
    };
    std::map<std::string, uint32_t> attribute_name_to_binding = {
        {"POSITION", 0},
        {"NORMAL", 1},
        {"TEXCOORD_0", 2},
        {"TANGENT", 3},
    };
    std::map<uint32_t, Morpho::Vulkan::Pipeline> pipeline_cache;

    void main_loop();
    void initialize_static_resources(Morpho::Vulkan::CommandBuffer& cmd);
    void init_window();
    void cleanup();
    void render_frame();
    void initialize_key_map();
    void update(float delta);
    Key glfw_key_code_to_key(int code);
    void create_scene_resources(Morpho::Vulkan::CommandBuffer& cmd);
    void draw_model(
        const tinygltf::Model& model,
        Morpho::Vulkan::CommandBuffer& cmd,
        const Morpho::Vulkan::Pipeline& normal_pipeline,
        const Morpho::Vulkan::Pipeline& double_sided_pipeline
    );
    void draw_scene(
        const tinygltf::Model& model,
        const tinygltf::Scene& scene,
        Morpho::Vulkan::CommandBuffer& cmd,
        const Morpho::Vulkan::Pipeline& normal_pipeline,
        const Morpho::Vulkan::Pipeline& double_sided_pipeline
    );
    void draw_node(
        const tinygltf::Model& model,
        const tinygltf::Node& node,
        Morpho::Vulkan::CommandBuffer& cmd,
        const Morpho::Vulkan::Pipeline& normal_pipeline,
        const Morpho::Vulkan::Pipeline& double_sided_pipeline
    );
    void draw_mesh(
        const tinygltf::Model& model,
        uint32_t mesh_index,
        Morpho::Vulkan::CommandBuffer& cmd,
        const Morpho::Vulkan::Pipeline& normal_pipeline,
        const Morpho::Vulkan::Pipeline& double_sided_pipeline
    );
    void draw_primitive(
        const tinygltf::Model& model,
        uint32_t mesh_index,
        uint32_t primitive_index,
        Morpho::Vulkan::CommandBuffer& cmd,
        const Morpho::Vulkan::Pipeline& normal_pipeline,
        const Morpho::Vulkan::Pipeline& double_sided_pipeline
    );
    void render_depth_pass_for_spot_light(
        Morpho::Vulkan::CommandBuffer& cmd,
        const Light& spot_ligt
    );
    void render_depth_pass_for_point_light(
        Morpho::Vulkan::CommandBuffer& cmd,
        const Light& point_light
    );
    void render_z_prepass(Morpho::Vulkan::CommandBuffer& cmd);
    void begin_color_pass(Morpho::Vulkan::CommandBuffer& cmd);
    void render_color_pass_for_spotlight(
        Morpho::Vulkan::CommandBuffer& cmd,
        const Light& light
    );
    void render_color_pass_for_point_light(
        Morpho::Vulkan::CommandBuffer& cmd,
        const Light& light
    );
    void add_light(Light light);
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
