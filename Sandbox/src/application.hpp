#pragma once

#include "imgui_impl_vulkan.h"
#include "src/rendering_utils/allocators.hpp"
#include "vulkan/resources.hpp"
#include <vulkan/vulkan_core.h>
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vulkan/context.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>
#include "input/input.hpp"
#include "camera.hpp"
#include <tiny_gltf.h>
#include <filesystem>
#include "vulkan/resource_manager.hpp"
#include "common/draw_stream.hpp"
#include "common/frame_pool.hpp"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct ViewProjection {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct Globals {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 camera_position;
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
    // 24 bytes
    glm::vec4 base_color_factor;
    float metalness_factor;
    float roughness_factor;
    // 256 - 24 bytes
    uint32_t padding[58];
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
    DirectionalLight,
};

struct Light {
    LightType light_type;
    uint32_t descriptor_set_start_index;
    Morpho::Handle<Morpho::Vulkan::Texture> shadow_map;
    Morpho::Handle<Morpho::Vulkan::Texture> views[6];
    union LightData {
        SpotLight spot_light;
        PointLight point_light;
    } light_data;
};

const uint32_t cascade_count = 3;

struct CsmUniform {
    glm::mat4 first_cascade_view_proj;
    glm::vec4 ranges[cascade_count];
    glm::vec4 offsets[cascade_count];
    glm::vec4 scales[cascade_count];
};

struct ModelUniform {
    glm::mat4 transform;
    glm::mat4 inverse_transpose_transform;
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
    // Stick with depth only format for now to avoid creating view by aspect.
    // (For depth + stencil format attachment requires both DEPTH and STENCIL even if stencil is not used,
    // but to sample depth texture we need view with only DEPTH aspect,
    // perhaps there should be RenderTarget structure that creates all necessary views upfront)
    static const VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
    static const VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

    Input input;
    GLFWwindow* window;
    uint32_t window_width = 1200;
    uint32_t window_height = 1000;
    Key key_map[Input::MAX_KEY_COUNT];
    Morpho::Vulkan::Context* context;
    Morpho::Vulkan::ResourceManager* resource_manager;

    Morpho::Handle<Morpho::Vulkan::RenderPassLayout> color_pass_layout;
    Morpho::Handle<Morpho::Vulkan::RenderPassLayout> depth_pass_layout;
    Morpho::Handle<Morpho::Vulkan::RenderPass> color_pass;
    Morpho::Handle<Morpho::Vulkan::RenderPass> depth_pass;
    Morpho::Handle<Morpho::Vulkan::RenderPass> imgui_pass;
    Morpho::Handle<Morpho::Vulkan::PipelineLayout> light_pipeline_layout;
    Morpho::Handle<Morpho::Vulkan::Pipeline> depth_pass_pipeline_ccw_depth_clamp;
    Morpho::Handle<Morpho::Vulkan::Pipeline> depth_pass_pipeline_ccw_depth_clamp_double_sided;
    Morpho::Handle<Morpho::Vulkan::Pipeline> depth_pass_pipeline_ccw;
    Morpho::Handle<Morpho::Vulkan::Pipeline> depth_pass_pipeline_ccw_double_sided;
    Morpho::Handle<Morpho::Vulkan::Pipeline> depth_pass_pipeline_cw;
    Morpho::Handle<Morpho::Vulkan::Pipeline> depth_pass_pipeline_cw_double_sided;
    Morpho::Handle<Morpho::Vulkan::Pipeline> spotlight_pipeline;
    Morpho::Handle<Morpho::Vulkan::Pipeline> spotlight_pipeline_double_sided;
    Morpho::Handle<Morpho::Vulkan::Pipeline> pointlight_pipeline;
    Morpho::Handle<Morpho::Vulkan::Pipeline> pointlight_pipeline_double_sided;
    Morpho::Handle<Morpho::Vulkan::Pipeline> directional_light_pipeline;
    Morpho::Handle<Morpho::Vulkan::Pipeline> directional_light_pipeline_double_sided;
    Morpho::Handle<Morpho::Vulkan::Pipeline> no_light_pipeline;
    Morpho::Handle<Morpho::Vulkan::Pipeline> no_light_pipeline_double_sided;
    Morpho::Handle<Morpho::Vulkan::Pipeline> shadow_map_visualization_pipeline;
    Morpho::Handle<Morpho::Vulkan::Pipeline> z_prepass_pipeline;
    Morpho::Handle<Morpho::Vulkan::Pipeline> z_prepass_pipeline_double_sided;
    Morpho::Handle<Morpho::Vulkan::Shader> z_prepass_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> gltf_depth_pass_vertex_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> gltf_spot_light_vertex_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> gltf_point_light_vertex_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> gltf_spot_light_fragment_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> gltf_point_light_fragment_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> gltf_directional_light_vertex_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> gltf_directional_light_fragment_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> no_light_vertex_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> no_light_fragment_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> full_screen_triangle_shader;
    Morpho::Handle<Morpho::Vulkan::Shader> shadow_map_spot_light_fragment_shader;
    Morpho::Handle<Morpho::Vulkan::Sampler> default_sampler;
    Morpho::Handle<Morpho::Vulkan::Sampler> shadow_sampler;
    Morpho::Handle<Morpho::Vulkan::Texture> white_texture;
    Morpho::Handle<Morpho::Vulkan::Texture> depth_buffer;
    std::vector<Morpho::Handle<Morpho::Vulkan::Buffer>> buffers;
    std::vector<Morpho::Handle<Morpho::Vulkan::Texture>> textures;
    std::vector<Morpho::Handle<Morpho::Vulkan::Sampler>> samplers;
    Morpho::Handle<Morpho::Vulkan::Buffer> globals_buffer;
    FixedSizeAllocator globals_allocator;
    Morpho::Handle<Morpho::Vulkan::DescriptorSet> global_descriptor_sets[frame_in_flight_count];
    Morpho::Handle<Morpho::Vulkan::Buffer> material_buffer;
    FixedSizeAllocator material_buffer_allocator;
    std::vector<Morpho::Handle<Morpho::Vulkan::DescriptorSet>> material_descriptor_sets;
    std::vector<Morpho::Handle<Morpho::Vulkan::DescriptorSet>> light_descriptor_sets;
    Morpho::Handle<Morpho::Vulkan::DescriptorSet> shadow_map_visualization_descriptor_set[frame_in_flight_count];
    Morpho::Handle<Morpho::Vulkan::Buffer> mesh_uniforms;
    FixedSizeAllocator mesh_uniforms_allocator;
    std::vector<Morpho::Handle<Morpho::Vulkan::DescriptorSet>> mesh_descriptor_sets;
    std::vector<Morpho::Handle<Morpho::Vulkan::DescriptorSet>> cube_map_face_descriptor_sets;
    uint32_t frames_total = 0;
    uint32_t frame_index = 0;
    std::vector<Light> lights;
    DirectionalLight sun { glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f)), glm::vec3(1.0f, 1.0f, 1.0f) };
    Morpho::Handle<Morpho::Vulkan::Texture> cascaded_shadow_maps;
    Morpho::Handle<Morpho::Vulkan::DescriptorSet> csm_descriptor_sets[frame_in_flight_count];
    Morpho::Handle<Morpho::Vulkan::Texture> directional_shadow_maps[cascade_count];
    Morpho::Handle<Morpho::Vulkan::DescriptorSet> directional_shadow_map_descriptor_sets[frame_in_flight_count * cascade_count];
    Morpho::FramePool<Morpho::DrawStream*> draw_stream_pool;
    UniformBufferBumpAllocator per_frame_uniforms;

    bool debug_mode = false;
    uint32_t current_light_index = 0;
    // cube face or array index
    uint32_t current_slice_index = 0;
    LightType current_light_type = LightType::DirectionalLight;

    bool is_first_update = true;
    Camera camera;
    const float camera_sensetivity = 0.1f;
    glm::vec3 world_up = { 0.0f, 1.0f, 0.0f};
    glm::vec2 last_mouse_position = { 400, 300 };
    bool is_mouse_pressed = false;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    int current_material_index = -1;
    Morpho::Handle<Morpho::Vulkan::Pipeline> currently_bound_pipeline = Morpho::Handle<Morpho::Vulkan::Pipeline>::null();
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
    std::vector<Morpho::Vulkan::TextureBarrier> texture_barriers;

    void main_loop();
    void begin_frame(Morpho::Vulkan::CommandBuffer* cmd);
    void transition_shadow_maps(Morpho::Vulkan::CommandBuffer* cmd);
    void initialize_static_resources(Morpho::Vulkan::CommandBuffer* cmd);
    void init_window();
    void init_imgui();
    void cleanup();
    void render_frame();
    void update_light_uniforms();
    void initialize_key_map();
    void update(float delta);
    void gui(float delta);
    void render_gui(Morpho::Vulkan::CommandBuffer* cmd);
    void calculate_cascades();
    Key glfw_key_code_to_key(int code);
    void generate_mipmaps(Morpho::Vulkan::CommandBuffer* cmd);
    void draw_model(
        const tinygltf::Model& model,
        Morpho::DrawStream* draw_stream,
        Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
        Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
    );
    void draw_scene(
        const tinygltf::Model& model,
        const tinygltf::Scene& scene,
        Morpho::DrawStream* draw_stream,
        Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
        Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
    );
    void draw_node(
        const tinygltf::Model& model,
        const tinygltf::Node& node,
        Morpho::DrawStream* draw_stream,
        Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
        Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
    );
    void draw_mesh(
        const tinygltf::Model& model,
        uint32_t mesh_index,
        Morpho::DrawStream* draw_stream,
        Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
        Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
    );
    void draw_primitive(
        const tinygltf::Model& model,
        uint32_t mesh_index,
        uint32_t primitive_index,
        Morpho::DrawStream* draw_stream,
        Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
        Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
    );
    void render_depth_pass_for_spot_light(
        Morpho::Vulkan::CommandBuffer* cmd,
        const Light& spot_ligt
    );
    void render_depth_pass_for_directional_light(Morpho::Vulkan::CommandBuffer* cmd);
    void render_z_prepass(Morpho::DrawStream* draw_stream);
    void begin_color_pass(Morpho::Vulkan::CommandBuffer* cmd);
    void render_color_pass_for_directional_light(Morpho::DrawStream* stream);
    void render_color_pass_for_spotlight(
        Morpho::DrawStream* stream,
        const Light& light
    );
    void add_light(Light light);
    Morpho::Handle<Morpho::Vulkan::Shader> load_shader(const std::string& path);
    static void process_keyboard_input(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void process_cursor_position(GLFWwindow* window, double xpos, double ypos);
    static void process_mouse_button_input(GLFWwindow* window, int button, int action, int mods);
    static VkFormat gltf_to_format(int type, int component_type);
    static VkIndexType gltf_to_index_type(int type, int component_type);
    static VkFilter gltf_to_filter(int filter);
    static VkSamplerAddressMode gltf_to_sampler_address_mode(int address_mode);
    std::vector<char> read_file(const std::string& filename);
};
