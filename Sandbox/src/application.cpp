#include "application.hpp"
#include <fstream>
#include <stb_image.h>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "math.hpp"

namespace fs = std::filesystem;

void Application::init()
{
    gltf_depth_pass_vertex_shader = load_shader("./assets/shaders/gltf_depth_pass.vert.spv");
    gltf_spot_light_vertex_shader = load_shader("./assets/shaders/gltf_spot_light.vert.spv");
    gltf_point_light_vertex_shader = load_shader("./assets/shaders/gltf_point_light.vert.spv");
    gltf_spot_light_fragment_shader = load_shader("./assets/shaders/gltf_spot_light.frag.spv");
    gltf_point_light_fragment_shader = load_shader("./assets/shaders/gltf_point_light.frag.spv");
    full_screen_triangle_shader = load_shader("./assets/shaders/full_screen_triangle.vert.spv");
    shadow_map_spot_light_fragment_shader = load_shader("./assets/shaders/shadow_map_spot_light.frag.spv");
    no_light_vertex_shader = load_shader("./assets/shaders/no_light.vert.spv");
    no_light_fragment_shader = load_shader("./assets/shaders/no_light.frag.spv");

    using namespace Morpho::Vulkan;

    color_pass_layout = context->acquire_render_pass_layout(RenderPassLayoutInfoBuilder()
        .attachment(VK_FORMAT_D16_UNORM)
        .attachment(context->get_swapchain_format())
        .subpass({1}, 0)
        .info()
    );

    depth_pass_layout = context->acquire_render_pass_layout(RenderPassLayoutInfoBuilder()
        .attachment(VK_FORMAT_D16_UNORM)
        .subpass({}, 0)
        .info()
    );

    depth_pass = context->acquire_render_pass(RenderPassInfoBuilder()
        .layout(depth_pass_layout)
        .attachment(
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ).info()
    );

    color_pass = context->acquire_render_pass(Morpho::Vulkan::RenderPassInfoBuilder()
        .layout(color_pass_layout)
        .attachment(
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ).attachment(
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        ).info()
    );

    VkDescriptorSetLayoutBinding set0_bindings[16] = {};
    VkDescriptorSetLayoutBinding set1_bindings[16] = {};
    PipelineLayoutInfo pipeline_layout_info{};
    pipeline_layout_info.set_binding_infos[0] = set0_bindings;
    pipeline_layout_info.set_binding_infos[1] = set1_bindings;
    {
        for (uint32_t i = 0; i < 3; i++) {
            set0_bindings[i] = { i, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, };
        }
        pipeline_layout_info.set_binding_count[0] = 3;

        for (uint32_t i = 0; i < 3; i++) {
            set1_bindings[i] = { i, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, };
        }

        for (uint32_t i = 3; i < 6; i++) {
            set1_bindings[i] = { i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, };
        }
        pipeline_layout_info.set_binding_count[1] = 6;
        light_pipeline_layout = context->create_pipeline_layout(pipeline_layout_info);
    }
    {
        pipeline_layout_info.set_binding_count[0] = 2;
        pipeline_layout_info.set_binding_count[1] = 0;
        pipeline_layout_info.set_binding_infos[1] = nullptr;
        set0_bindings[0] = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, };
        set0_bindings[1] = { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, };
        shadow_map_debug_pipeline_layout = context->create_pipeline_layout(pipeline_layout_info);
    }

    PipelineInfo pipeline_info{};
    VkVertexInputAttributeDescription attributes[4];
    attributes[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0, }; // Position
    attributes[1] = { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0, }; // Normal
    attributes[2] = { 2, 2, VK_FORMAT_R32G32_SFLOAT, 0, }; // Texture coordinates
    attributes[3] = { 3, 3, VK_FORMAT_R32G32B32A32_SFLOAT, 0, }; // Tangent
    VkVertexInputBindingDescription bindings[4];
    bindings[0] = { 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX, };
    bindings[1] = { 1, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX, };
    bindings[2] = { 2, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX, };
    bindings[3] = { 3, sizeof(float) * 4, VK_VERTEX_INPUT_RATE_VERTEX, };
    Shader shaders[2];
    pipeline_info.attributes = attributes;
    pipeline_info.bindings = bindings;
    pipeline_info.shaders = shaders;
    pipeline_info.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineColorBlendAttachmentState no_blend{};
    no_blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendAttachmentState additive =  {
        VK_TRUE,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    // light
    pipeline_info.shader_count = 2;
    // TODO Additive and Z-Prepass
    pipeline_info.blend_state = no_blend;
    pipeline_info.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipeline_info.render_pass_layout = &color_pass_layout;
    pipeline_info.pipeline_layout = &light_pipeline_layout;
    pipeline_info.attribute_count = 4;
    pipeline_info.binding_count = 4;
    pipeline_info.depth_test_enabled = true;
    pipeline_info.depth_write_enabled = true;
    pipeline_info.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    {
        // Spot light shading.
        pipeline_info.shaders[0] = gltf_spot_light_vertex_shader;
        pipeline_info.shaders[1] = gltf_spot_light_fragment_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        spotlight_pipeline = context->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        spotlight_pipeline_double_sided = context->create_pipeline(pipeline_info);
    }
    {
        // Point light shading.
        pipeline_info.shaders[0] = gltf_point_light_vertex_shader;
        pipeline_info.shaders[1] = gltf_point_light_fragment_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        pointlight_pipeline = context->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        pointlight_pipeline_double_sided = context->create_pipeline(pipeline_info);
    }
    pipeline_info.blend_state = no_blend;
    {
        // No light shading.
        pipeline_info.shaders[0] = no_light_vertex_shader;
        pipeline_info.shaders[1] = no_light_fragment_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        no_light_pipeline = context->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        no_light_pipeline_double_sided = context->create_pipeline(pipeline_info);
    }
    {
        // Depth pass.
        pipeline_info.depth_bias_constant_factor = 4.0f;
        pipeline_info.depth_bias_slope_factor = 1.5f;
        pipeline_info.shader_count = 1;
        pipeline_info.shaders[0] = gltf_depth_pass_vertex_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        pipeline_info.render_pass_layout = &depth_pass_layout;
        depth_pass_pipeline_ccw = context->create_pipeline(pipeline_info);
        pipeline_info.front_face = VK_FRONT_FACE_CLOCKWISE;
        depth_pass_pipeline_cw = context->create_pipeline(pipeline_info);
    }
    pipeline_info.depth_test_enabled = false;
    pipeline_info.depth_write_enabled = false;
    pipeline_info.depth_bias_constant_factor = pipeline_info.depth_bias_slope_factor = 0.0f;
    {
        // Shadow map debug
        pipeline_info.attribute_count = 0;
        pipeline_info.binding_count = 0;
        pipeline_info.render_pass_layout = &color_pass_layout;
        pipeline_info.shader_count = 2;
        pipeline_info.shaders[0] = full_screen_triangle_shader;
        pipeline_info.shaders[1] = shadow_map_spot_light_fragment_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        pipeline_info.pipeline_layout = &shadow_map_debug_pipeline_layout;
        shadow_map_visualization_pipeline = context->create_pipeline(pipeline_info);
    }

    default_sampler = context->acquire_sampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_FILTER_LINEAR);
    shadow_sampler = context->acquire_sampler(
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        VK_FILTER_LINEAR,
        VK_TRUE,
        VK_COMPARE_OP_LESS
    );
}

void Application::run()
{
    init_window();
    initialize_key_map();
    context->init(window);
    context->set_frame_context_count(2);
    auto swapchain_extent = context->get_swapchain_extent();
    camera = Camera(
        0.0f,
        0.0f,
        glm::vec3(0, 1, 0),
        glm::radians(60.0f),
        swapchain_extent.width / (float)swapchain_extent.height,
        0.01f,
        10.0f
    );
    camera.set_position(glm::vec3(0.0f, 1.0f, 0.0f));
    auto forward = camera.get_forward();
    main_loop();
}

void Application::init_window() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(1200, 1000, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, process_keyboard_input);
    glfwSetCursorPosCallback(window, process_cursor_position);
    glfwSetMouseButtonCallback(window, process_mouse_button_input);
}

void Application::main_loop() {
    auto last_frame = 0.0;
    while (!glfwWindowShouldClose(window)) {
        auto current_frame = glfwGetTime();
        auto delta = current_frame - last_frame;
        last_frame = current_frame;
        input.update();
        glfwPollEvents();
        update((float)delta);
        render_frame();
    }
}

void Application::set_graphics_context(Morpho::Vulkan::Context* context) {
    this->context = context;
}

void Application::render_frame() {
    context->begin_frame();
    auto cmd = context->acquire_command_buffer();
    if (is_first_update) {
        initialize_static_resources(cmd);
        is_first_update = false;
    }
    shadow_maps.clear();
    for (const SpotLight& spot_light : spot_lights) {
        shadow_maps.push_back(render_depth_pass(cmd, spot_light));
    }
    for (const PointLight& point_light : point_lights) {
        shadow_maps.push_back(render_depth_pass(cmd, point_light));
    }
    begin_color_pass(cmd);
    if (input.is_key_pressed(Key::LEFT_CONTROL)) {
        for (int i = 0; i < spot_lights.size(); i++) {
            if (i > 9) {
                break;
            }
            auto key = (Key)((int)Key::NUMBER0 + i);
            if (input.is_key_pressed(key)) {
                draw_depth_image(cmd, shadow_maps[i]);
            }
        }
    } else if (input.is_key_pressed(Key::LEFT_ALT) && !point_lights.empty()) {
        auto& pl = point_lights[0];
        for (int i = 0; i < 6; i++) {
            auto key = (Key)((int)Key::NUMBER0 + i);
            if (input.is_key_pressed(key)) {
                auto sm = shadow_maps[spot_lights.size()];
                auto tiv = context->create_temporary_image_view(
                    VK_FORMAT_D16_UNORM,
                    sm.image,
                    VK_IMAGE_ASPECT_DEPTH_BIT,
                    VK_IMAGE_VIEW_TYPE_2D,
                    i
                );
                draw_depth_image(cmd, tiv);
            }
        }
    } else {
        for (int i = 0; i < spot_lights.size(); i++) {
            render_color_pass(cmd, shadow_maps[i], spot_lights[i]);
        }
        for (int i = 0; i < point_lights.size(); i++) {
            render_color_pass(cmd, shadow_maps[spot_lights.size() + i], point_lights[i]);
        }
    }
    cmd.end_render_pass(); // color pass
    context->submit(cmd);
    context->end_frame();
}

Morpho::Vulkan::ImageView Application::render_depth_pass(
    Morpho::Vulkan::CommandBuffer& cmd,
    const SpotLight& spot_light
) {
    cmd.reset();
    auto vp_buffer = context->acquire_staging_buffer(
        sizeof(ViewProjection),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    auto extent = context->get_swapchain_extent();
    cmd.set_viewport({0, 0, (float)extent.width, (float)extent.height, 0.0f, 1.0f, });
    cmd.set_scissor({{0, 0}, extent});
    ViewProjection vp;
    vp.view = look_at(spot_light.position, spot_light.position + 10.0f * spot_light.direction, world_up);
    vp.proj = perspective(glm::radians(90.0f), extent.width / (float)extent.height, 0.01f, 100.0f);
    vp_buffer.update(&vp, sizeof(ViewProjection));
    cmd.set_uniform_buffer(0, 0, vp_buffer, 0, sizeof(ViewProjection));
    cmd.set_uniform_buffer(0, 2, vp_buffer, 0, sizeof(ViewProjection));
    cmd.bind_pipeline(depth_pass_pipeline_ccw);
    auto depth_image = context->acquire_temporary_image(
        { extent.width, extent.height, 1 },
        VK_FORMAT_D16_UNORM,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto depth_image_view = context->create_temporary_image_view(
        VK_FORMAT_D16_UNORM,
        depth_image,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    auto framebuffer = context->acquire_framebuffer(Morpho::Vulkan::FramebufferInfoBuilder()
        .layout(depth_pass_layout)
        .extent(extent)
        .attachment(depth_image_view)
        .info()
    );
    cmd.begin_render_pass(
        depth_pass,
        framebuffer,
        { {0, 0}, extent },
        {
            {1.0f, 0},
            {0.0f, 0.0f, 0.0f, 0.0f},
        }
    );
    draw_model(model, cmd);
    cmd.end_render_pass();
    cmd.image_barrier(
        depth_image,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );
    return depth_image_view;
}


Morpho::Vulkan::ImageView Application::render_depth_pass(
    Morpho::Vulkan::CommandBuffer& cmd,
    const PointLight& point_light
) {
    cmd.reset();
    cmd.bind_pipeline(depth_pass_pipeline_cw);
    auto extent = context->get_swapchain_extent();
    extent.width = extent.height = std::max(extent.width, extent.height);
    cmd.set_viewport({0, 0, (float)extent.width, (float)extent.height, 0.0f, 1.0f, });
    cmd.set_scissor({{0, 0}, extent});
    auto depth_image = context->acquire_temporary_image(
        { extent.width, extent.height, 1 },
        VK_FORMAT_D16_UNORM,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        6,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
    );
    auto x = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    auto y = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    auto z = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    auto w = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::mat4 faces[6] = {
        glm::mat4(-z, -y, x, w),
        glm::mat4(z, -y, -x, w),
        glm::mat4(x, z, y, w),
        glm::mat4(x, -z, -y, w),
        glm::mat4(x, -y, z, w),
        glm::mat4(-x, -y, -z, w),
    };
    glm::vec3 dirs[6] = {
        x, -x, y, -y, z, -z,
    };
    ViewProjection vp;
    vp.proj = perspective(glm::radians(90.0f), extent.width / (float)extent.height, 0.01f, 100.0f);
    for (uint32_t i = 0; i < 6; i++) {
        auto vp_buffer = context->acquire_staging_buffer(
            sizeof(ViewProjection),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY
        );
        auto rot = faces[i];
        auto translate = glm::translate(glm::mat4(1.0f), point_light.position);
        auto combined = translate * rot;
        vp.view = glm::inverse(combined);
        auto det = glm::determinant(vp.view);
        vp_buffer.update(&vp, sizeof(ViewProjection));
        cmd.set_uniform_buffer(0, 0, vp_buffer, 0, sizeof(ViewProjection));
        cmd.set_uniform_buffer(0, 2, vp_buffer, 0, sizeof(ViewProjection));
        auto depth_image_view = context->create_temporary_image_view(
            VK_FORMAT_D16_UNORM,
            depth_image,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_VIEW_TYPE_2D,
            i
        );
        auto framebuffer = context->acquire_framebuffer(Morpho::Vulkan::FramebufferInfoBuilder()
            .layout(depth_pass_layout)
            .extent(extent)
            .attachment(depth_image_view)
            .info()
        );
        cmd.begin_render_pass(
            depth_pass,
            framebuffer,
            { {0, 0}, extent },
            {
                {1.0f, 0},
                {0.0f, 0.0f, 0.0f, 0.0f},
            }
        );
        draw_model(model, cmd);
        cmd.end_render_pass();
    }
    cmd.image_barrier(
        depth_image,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        6
    );
    return context->create_temporary_image_view(
        VK_FORMAT_D16_UNORM,
        depth_image,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE,
        0,
        6
    );
}

void Application::begin_color_pass(Morpho::Vulkan::CommandBuffer& cmd) {
    cmd.reset();
    auto extent = context->get_swapchain_extent();
    cmd.set_viewport({0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f, });
    cmd.set_scissor({{0, 0}, extent});
    auto depth_image = context->acquire_temporary_image(
        { extent.width, extent.height, 1 },
        VK_FORMAT_D16_UNORM,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto depth_image_view = context->create_temporary_image_view(
        VK_FORMAT_D16_UNORM,
        depth_image,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    auto framebuffer = context->acquire_framebuffer(Morpho::Vulkan::FramebufferInfoBuilder()
        .layout(color_pass_layout)
        .extent(extent)
        .attachment(depth_image_view)
        .attachment(context->get_swapchain_image_view())
        .info()
    );
    cmd.begin_render_pass(
        color_pass,
        framebuffer,
        { {0, 0}, extent },
        {
            {1.0f, 0},
            {0.0f, 0.0f, 0.0f, 0.0f},
        }
    );
}

void Application::render_color_pass(
    Morpho::Vulkan::CommandBuffer& cmd,
    Morpho::Vulkan::ImageView shadow_map,
    const PointLight& point_light
) {
    setup_point_light_uniforms(cmd, point_light);
    auto vp_buffer = context->acquire_staging_buffer(
        sizeof(ViewProjection),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    auto lvp_buffer = context->acquire_staging_buffer(
        sizeof(ViewProjection),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    ViewProjection vp;
    vp.view = camera.get_view();
    vp.proj = camera.get_projection();
    vp_buffer.update(&vp, sizeof(ViewProjection));
    cmd.set_uniform_buffer(0, 0, vp_buffer, 0, sizeof(ViewProjection));
    VkExtent2D extent = context->get_swapchain_extent();
    extent.width = extent.height = std::max(extent.width, extent.height);
    vp.view = glm::translate(glm::mat4(1.0f), -point_light.position);
    vp.proj = perspective(glm::radians(90.0f), extent.width / (float)extent.height, 0.01f, 100.0f);
    lvp_buffer.update(&vp, sizeof(ViewProjection));
    cmd.set_uniform_buffer(0, 2, lvp_buffer, 0, sizeof(ViewProjection));
    cmd.bind_pipeline(pointlight_pipeline_double_sided);
    cmd.set_combined_image_sampler(1, 5, shadow_map, shadow_sampler, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    draw_model(model, cmd);
}

void Application::render_color_pass(
    Morpho::Vulkan::CommandBuffer& cmd,
    Morpho::Vulkan::ImageView shadow_map,
    const SpotLight& spot_light
) {
    setup_spot_light_uniforms(cmd, spot_light);
    auto vp_buffer = context->acquire_staging_buffer(
        sizeof(ViewProjection),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    auto lvp_buffer = context->acquire_staging_buffer(
        sizeof(ViewProjection),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    ViewProjection vp;
    vp.view = camera.get_view();
    vp.proj = camera.get_projection();
    vp_buffer.update(&vp, sizeof(ViewProjection));
    cmd.set_uniform_buffer(0, 0, vp_buffer, 0, sizeof(ViewProjection));
    VkExtent2D extent = context->get_swapchain_extent();
    ViewProjection lvp;
    lvp.view = look_at(spot_light.position, spot_light.position + 10.0f * spot_light.direction, world_up);
    lvp.proj = perspective(glm::radians(90.0f), extent.width / (float)extent.height, 0.01f, 100.0f);
    lvp_buffer.update(&lvp, sizeof(ViewProjection));
    cmd.set_uniform_buffer(0, 2, lvp_buffer, 0, sizeof(ViewProjection));
    cmd.bind_pipeline(spotlight_pipeline_double_sided);
    cmd.set_combined_image_sampler(1, 5, shadow_map, shadow_sampler, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    draw_model(model, cmd);
}

void Application::draw_depth_image(Morpho::Vulkan::CommandBuffer &cmd, Morpho::Vulkan::ImageView depth_map) {
    cmd.reset();
    cmd.bind_pipeline(shadow_map_visualization_pipeline);
    cmd.set_combined_image_sampler(0, 0, depth_map, default_sampler, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    float projection_params[2] = { 0.01f, 10.0f, };
    auto projection_params_buffer = context->acquire_staging_buffer(
        sizeof(projection_params),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    projection_params_buffer.update(projection_params, sizeof(projection_params));
    cmd.set_uniform_buffer(0, 1, projection_params_buffer, 0, sizeof(projection_params));
    cmd.draw(3, 1, 0, 0);
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

void Application::initialize_static_resources(Morpho::Vulkan::CommandBuffer& cmd) {
    uint32_t white_pixel = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> white_pixels(1 * 1, white_pixel);
    white_image = context->acquire_image(
        { 1, 1, 1 },
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    auto white_staging_buffer = context->acquire_staging_buffer(
        1 * 1 * 4,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    white_staging_buffer.update(white_pixels.data(), 1 * 1 * 4);
    cmd.image_barrier(
        white_image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT
    );
    cmd.copy_buffer_to_image(white_staging_buffer, white_image, { (uint32_t)1, (uint32_t)1, (uint32_t)1 });
    cmd.image_barrier(
        white_image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );
    white_image_view = context->create_image_view(VK_FORMAT_R8G8B8A8_UNORM, white_image, VK_IMAGE_ASPECT_COLOR_BIT);
    create_scene_resources(cmd);
}

void Application::process_keyboard_input(GLFWwindow* window, int key_code, int scancode, int action, int mods) {
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    auto key = app->glfw_key_code_to_key(key_code);
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        app->input.press_key(key);
    } else if (action == GLFW_RELEASE) {
        app->input.release_key(key);
    }
}

void Application::process_cursor_position(GLFWwindow* window, double xpos, double ypos) {
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    app->input.set_mouse_position((float)xpos, (float)ypos);
}

void Application::process_mouse_button_input(GLFWwindow* window, int button, int action, int mods)
{
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    Key keys[GLFW_MOUSE_BUTTON_LAST] = { Key::UNDEFINED, };
    keys[GLFW_MOUSE_BUTTON_LEFT] = Key::MOUSE_BUTTON_LEFT;
    keys[GLFW_MOUSE_BUTTON_RIGHT] = Key::MOUSE_BUTTON_RIGHT;
    keys[GLFW_MOUSE_BUTTON_MIDDLE] = Key::MOUSE_BUTTON_MIDDLE;
    if (keys[button] == Key::UNDEFINED) {
        return;
    }
    if (action == GLFW_PRESS) {
        app->input.press_key(keys[button]);
    } else {
        app->input.release_key(keys[button]);
    }
}


void Application::initialize_key_map() {
    for (uint32_t i = 0; i < Input::MAX_KEY_COUNT; i++) {
        key_map[i] = Key::UNDEFINED;
    }
    key_map[GLFW_KEY_SPACE] = Key::SPACE;
    key_map[GLFW_KEY_APOSTROPHE] = Key::APOSTROPHE;
    key_map[GLFW_KEY_COMMA] = Key::COMMA;
    key_map[GLFW_KEY_MINUS] = Key::MINUS;
    key_map[GLFW_KEY_PERIOD] = Key::PERIOD;
    key_map[GLFW_KEY_SLASH] = Key::SLASH;
    key_map[GLFW_KEY_0] = Key::NUMBER0;
    key_map[GLFW_KEY_1] = Key::NUMBER1;
    key_map[GLFW_KEY_2] = Key::NUMBER2;
    key_map[GLFW_KEY_3] = Key::NUMBER3;
    key_map[GLFW_KEY_4] = Key::NUMBER4;
    key_map[GLFW_KEY_5] = Key::NUMBER5;
    key_map[GLFW_KEY_6] = Key::NUMBER6;
    key_map[GLFW_KEY_7] = Key::NUMBER7;
    key_map[GLFW_KEY_8] = Key::NUMBER8;
    key_map[GLFW_KEY_9] = Key::NUMBER9;
    key_map[GLFW_KEY_SEMICOLON] = Key::SEMICOLON;
    key_map[GLFW_KEY_EQUAL] = Key::EQUAL;
    key_map[GLFW_KEY_A] = Key::A;
    key_map[GLFW_KEY_B] = Key::B;
    key_map[GLFW_KEY_C] = Key::C;
    key_map[GLFW_KEY_D] = Key::D;
    key_map[GLFW_KEY_E] = Key::E;
    key_map[GLFW_KEY_F] = Key::F;
    key_map[GLFW_KEY_G] = Key::G;
    key_map[GLFW_KEY_H] = Key::H;
    key_map[GLFW_KEY_I] = Key::I;
    key_map[GLFW_KEY_J] = Key::J;
    key_map[GLFW_KEY_K] = Key::K;
    key_map[GLFW_KEY_L] = Key::L;
    key_map[GLFW_KEY_M] = Key::M;
    key_map[GLFW_KEY_N] = Key::N;
    key_map[GLFW_KEY_O] = Key::O;
    key_map[GLFW_KEY_P] = Key::P;
    key_map[GLFW_KEY_Q] = Key::Q;
    key_map[GLFW_KEY_R] = Key::R;
    key_map[GLFW_KEY_S] = Key::S;
    key_map[GLFW_KEY_T] = Key::T;
    key_map[GLFW_KEY_U] = Key::U;
    key_map[GLFW_KEY_V] = Key::V;
    key_map[GLFW_KEY_W] = Key::W;
    key_map[GLFW_KEY_X] = Key::X;
    key_map[GLFW_KEY_Y] = Key::Y;
    key_map[GLFW_KEY_Z] = Key::Z;
    key_map[GLFW_KEY_LEFT_BRACKET] = Key::LEFT_BRACKET;
    key_map[GLFW_KEY_BACKSLASH] = Key::BACKSLASH;
    key_map[GLFW_KEY_RIGHT_BRACKET] = Key::RIGHT_BRACKET;
    key_map[GLFW_KEY_GRAVE_ACCENT] = Key::GRAVE_ACCENT;
    key_map[GLFW_KEY_WORLD_1] = Key::WORLD_1;
    key_map[GLFW_KEY_WORLD_2] = Key::WORLD_2;
    key_map[GLFW_KEY_ESCAPE] = Key::ESCAPE;
    key_map[GLFW_KEY_ENTER] = Key::ENTER;
    key_map[GLFW_KEY_TAB] = Key::TAB;
    key_map[GLFW_KEY_BACKSPACE] = Key::BACKSPACE;
    key_map[GLFW_KEY_INSERT] = Key::INSERT;
    key_map[GLFW_KEY_DELETE] = Key::DEL;
    key_map[GLFW_KEY_RIGHT] = Key::RIGHT;
    key_map[GLFW_KEY_LEFT] = Key::LEFT;
    key_map[GLFW_KEY_DOWN] = Key::DOWN;
    key_map[GLFW_KEY_UP] = Key::UP;
    key_map[GLFW_KEY_PAGE_UP] = Key::PAGE_UP;
    key_map[GLFW_KEY_PAGE_DOWN] = Key::PAGE_DOWN;
    key_map[GLFW_KEY_HOME] = Key::HOME;
    key_map[GLFW_KEY_END] = Key::END;
    key_map[GLFW_KEY_CAPS_LOCK] = Key::CAPS_LOCK;
    key_map[GLFW_KEY_SCROLL_LOCK] = Key::SCROLL_LOCK;
    key_map[GLFW_KEY_NUM_LOCK] = Key::NUM_LOCK;
    key_map[GLFW_KEY_PRINT_SCREEN] = Key::PRINT_SCREEN;
    key_map[GLFW_KEY_PAUSE] = Key::PAUSE;
    key_map[GLFW_KEY_F1] = Key::F1;
    key_map[GLFW_KEY_F2] = Key::F2;
    key_map[GLFW_KEY_F3] = Key::F3;
    key_map[GLFW_KEY_F4] = Key::F4;
    key_map[GLFW_KEY_F5] = Key::F5;
    key_map[GLFW_KEY_F6] = Key::F6;
    key_map[GLFW_KEY_F7] = Key::F7;
    key_map[GLFW_KEY_F8] = Key::F8;
    key_map[GLFW_KEY_F9] = Key::F9;
    key_map[GLFW_KEY_F10] = Key::F10;
    key_map[GLFW_KEY_F11] = Key::F11;
    key_map[GLFW_KEY_F12] = Key::F12;
    key_map[GLFW_KEY_F13] = Key::F13;
    key_map[GLFW_KEY_F14] = Key::F14;
    key_map[GLFW_KEY_F15] = Key::F15;
    key_map[GLFW_KEY_F16] = Key::F16;
    key_map[GLFW_KEY_F17] = Key::F17;
    key_map[GLFW_KEY_F18] = Key::F18;
    key_map[GLFW_KEY_F19] = Key::F19;
    key_map[GLFW_KEY_F20] = Key::F20;
    key_map[GLFW_KEY_F21] = Key::F21;
    key_map[GLFW_KEY_F22] = Key::F22;
    key_map[GLFW_KEY_F23] = Key::F23;
    key_map[GLFW_KEY_F24] = Key::F24;
    key_map[GLFW_KEY_F25] = Key::F25;
    key_map[GLFW_KEY_KP_0] = Key::KP_0;
    key_map[GLFW_KEY_KP_1] = Key::KP_1;
    key_map[GLFW_KEY_KP_2] = Key::KP_2;
    key_map[GLFW_KEY_KP_3] = Key::KP_3;
    key_map[GLFW_KEY_KP_4] = Key::KP_4;
    key_map[GLFW_KEY_KP_5] = Key::KP_5;
    key_map[GLFW_KEY_KP_6] = Key::KP_6;
    key_map[GLFW_KEY_KP_7] = Key::KP_7;
    key_map[GLFW_KEY_KP_8] = Key::KP_8;
    key_map[GLFW_KEY_KP_9] = Key::KP_9;
    key_map[GLFW_KEY_KP_DECIMAL] = Key::KP_DECIMAL;
    key_map[GLFW_KEY_KP_DIVIDE] = Key::KP_DIVIDE;
    key_map[GLFW_KEY_KP_MULTIPLY] = Key::KP_MULTIPLY;
    key_map[GLFW_KEY_KP_SUBTRACT] = Key::KP_SUBTRACT;
    key_map[GLFW_KEY_KP_ADD] = Key::KP_ADD;
    key_map[GLFW_KEY_KP_ENTER] = Key::KP_ENTER;
    key_map[GLFW_KEY_KP_EQUAL] = Key::KP_EQUAL;
    key_map[GLFW_KEY_LEFT_SHIFT] = Key::LEFT_SHIFT;
    key_map[GLFW_KEY_LEFT_CONTROL] = Key::LEFT_CONTROL;
    key_map[GLFW_KEY_LEFT_ALT] = Key::LEFT_ALT;
    key_map[GLFW_KEY_LEFT_SUPER] = Key::LEFT_SUPER;
    key_map[GLFW_KEY_RIGHT_SHIFT] = Key::RIGHT_SHIFT;
    key_map[GLFW_KEY_RIGHT_CONTROL] = Key::RIGHT_CONTROL;
    key_map[GLFW_KEY_RIGHT_ALT] = Key::RIGHT_ALT;
    key_map[GLFW_KEY_RIGHT_SUPER] = Key::RIGHT_SUPER;
    key_map[GLFW_KEY_MENU] = Key::MENU;
}

Key Application::glfw_key_code_to_key(int code) {
    return code < 0 ? Key::UNDEFINED : key_map[code];
}

void Application::update(float delta) {
    glm::vec2 mouse_position(0, 0);
    input.get_mouse_position(&mouse_position.x, &mouse_position.y);
    if (input.is_key_pressed(Key::MOUSE_BUTTON_RIGHT)) {
        if (!is_mouse_pressed) {
            last_mouse_position = mouse_position;
            is_mouse_pressed = true;
        } else {
            auto mouse_delta = mouse_position - last_mouse_position;
            camera.add_yaw(camera_sensetivity * mouse_delta.x);
            camera.add_pitch(camera_sensetivity * mouse_delta.y);
            last_mouse_position = mouse_position;
        }
    } else {
        is_mouse_pressed = false;
    }

    auto shift_pressed = input.is_key_pressed(Key::LEFT_SHIFT);
    auto w_pressed = input.is_key_pressed(Key::W);
    auto a_pressed = input.is_key_pressed(Key::A);
    auto s_pressed = input.is_key_pressed(Key::S);
    auto d_pressed = input.is_key_pressed(Key::D);
    auto q_pressed = input.is_key_pressed(Key::Q);
    auto e_pressed = input.is_key_pressed(Key::E);

    auto camera_position = camera.get_position();
    auto face_move = (w_pressed ^ s_pressed) * (w_pressed ? 1 : -1);
    auto side_move = (a_pressed ^ d_pressed) * (d_pressed ? 1 : -1);
    auto vertical_move = (q_pressed ^ e_pressed) * (e_pressed ? 1 : -1);
    if (face_move != 0 || side_move != 0 || vertical_move != 0) {
        auto scale = shift_pressed ? 10.0f : 1.0f;
        camera_position += camera.get_forward() * (scale * delta * (float)face_move);
        camera_position += camera.get_right() * (scale * delta * (float)side_move);
        camera_position += world_up * (scale * delta * (float)vertical_move);
        camera.set_position(camera_position);
    }
    if (input.was_key_pressed(Key::F)) {
        spot_lights.push_back(SpotLight(
            camera.get_position(),
            camera.get_forward(),
            glm::vec3(1.0f, 0.0f, 0.0f),
            14.0f,
            3.0f,
            cos(glm::radians(17.5f)),
            cos(glm::radians(12.5f))
        ));
    }
    if (input.was_key_pressed(Key::G) && point_lights.size() == 0) {
        point_lights.push_back(PointLight(
            camera.get_position(),
            glm::vec3(1.0f),
            15.0f,
            2.0f
        ));
    }
    if (input.was_key_pressed(Key::P) && !point_lights.empty()) {
        point_lights[0].position = camera.get_position();
    }
    if (point_lights.size() > 0 && input.was_key_pressed(Key::G)) {
        point_lights[0].position = camera.get_position();
    }
    if (spot_lights.size() > 0) {
        spot_lights[0].position = camera.get_position();
        spot_lights[0].direction = camera.get_forward();
    }
    frames++;
}

bool Application::load_scene(std::filesystem::path file_path) {
    std::string err;
    std::string warn;
    if (file_path.extension() == ".gltf") {
        loader.LoadASCIIFromFile(&model, &err, &warn, file_path.string());
    } else {
        loader.LoadBinaryFromFile(&model, &err, &warn, file_path.string());
    }
    if (!warn.empty()) {
        std::cout << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << err << std::endl;
        return false;
    }
    return true;
}

void Application::create_scene_resources(Morpho::Vulkan::CommandBuffer& cmd) {
    std::vector<VkBufferUsageFlags> buffer_usages(model.buffers.size(), 0);
    buffers.resize(model.buffers.size());
    for (auto& mesh : model.meshes) {
        for (auto& primitive : mesh.primitives) {
            if (primitive.indices >= 0) {
                auto buffer_view_index = model.accessors[primitive.indices].bufferView;
                auto buffer_index = model.bufferViews[buffer_view_index].buffer;
                buffer_usages[buffer_index] |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            }
            for (auto& key_value : primitive.attributes) {
                auto buffer_view_index = model.accessors[key_value.second].bufferView;
                auto buffer_index = model.bufferViews[buffer_view_index].buffer;
                buffer_usages[buffer_index] |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            }
        }
    }
    for (uint32_t i = 0; i < model.buffers.size(); i++) {
        auto buffer_size = (VkDeviceSize)model.buffers[i].data.size();
        auto staging_buffer = context->acquire_staging_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY
        );
        staging_buffer.update(model.buffers[i].data.data(), buffer_size);
        buffers[i] = context->acquire_buffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | buffer_usages[i],
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        cmd.copy_buffer(staging_buffer, buffers[i], buffer_size);
        cmd.buffer_barrier(
            buffers[i],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            0,
            VK_WHOLE_SIZE
        );
    }
    texture_images.resize(model.textures.size());
    texture_image_views.resize(model.textures.size());
    for (uint32_t i = 0; i < model.textures.size(); i++) {
        auto& texture = model.textures[i];
        auto gltf_image = model.images[texture.source];
        // TODO: get format from bits + component + pixel_type
        assert(gltf_image.component == 4);
        auto image_size = (VkDeviceSize)(gltf_image.width * gltf_image.height * gltf_image.component * (gltf_image.bits / 8));
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        auto staging_image_buffer = context->acquire_staging_buffer(
            image_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY
        );
        staging_image_buffer.update(gltf_image.image.data(), image_size);
        auto image = context->acquire_image(
            { (uint32_t)gltf_image.width, (uint32_t)gltf_image.height, (uint32_t)1 },
            format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        cmd.image_barrier(
            image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT
        );
        cmd.copy_buffer_to_image(staging_image_buffer, image, { (uint32_t)gltf_image.width, (uint32_t)gltf_image.height, (uint32_t)1 });
        cmd.image_barrier(
            image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );
        texture_images[i] = image;
        auto image_view = context->create_image_view(
            format,
            image,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        texture_image_views[i] = image_view;
    }
    samplers.resize(model.samplers.size());
    for (uint32_t i = 0; i < model.samplers.size(); i++) {
        auto gltf_sampler = model.samplers[i];
        samplers[i] = context->acquire_sampler(
            gltf_to_sampler_address_mode(gltf_sampler.wrapT),
            gltf_to_sampler_address_mode(gltf_sampler.wrapR),
            gltf_to_sampler_address_mode(gltf_sampler.wrapS),
            gltf_to_filter(gltf_sampler.minFilter),
            gltf_to_filter(gltf_sampler.magFilter)
        );
    }
}

void Application::draw_model(const tinygltf::Model& model, Morpho::Vulkan::CommandBuffer& cmd) {
    current_material_index = -1;
    for (const auto& scene : model.scenes) {
        draw_scene(model, scene, cmd);
    }
}

void Application::draw_scene(const tinygltf::Model& model, const tinygltf::Scene& scene, Morpho::Vulkan::CommandBuffer& cmd) {
    for (const auto node_index : scene.nodes) {
        draw_node(model, model.nodes[node_index], cmd, glm::mat4(1.0f));
    }
}

void Application::draw_node(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    Morpho::Vulkan::CommandBuffer& cmd,
    glm::mat4 parent_to_world
) {
    glm::mat4 local_to_world = glm::mat4(1.0f);
    if (node.matrix.size() == 16) {
        auto& m = node.matrix;
        local_to_world = glm::mat4(
            m[0], m[1], m[2], m[3],
            m[4], m[5], m[6], m[7],
            m[8], m[9], m[10], m[11],
            m[12], m[13], m[14], m[15]
        );
    } else {
        if (node.translation.size() == 3) {
            auto& t = node.translation;
            local_to_world = glm::translate(
                local_to_world,
                glm::vec3(t[0], t[1], t[2])
            );
        }
        if (node.rotation.size() == 4) {
            auto& r = node.rotation;
            auto q = glm::quat((float)r[3], (float)r[2], (float)r[1], (float)r[0]);
            q = glm::make_quat(r.data());
            local_to_world = local_to_world * glm::mat4(q);
        }
        if (node.scale.size() == 3) {
            auto& s = node.scale;
            local_to_world = glm::scale(
                local_to_world,
                glm::vec3(s[0], s[1], s[2])
            );
        }
    }
    local_to_world = parent_to_world * local_to_world;
    if (node.mesh >= 0) {
        auto uniform_buffer = context->acquire_staging_buffer(
            sizeof(glm::mat4),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        uniform_buffer.update(&local_to_world, sizeof(glm::mat4));
        cmd.set_uniform_buffer(0, 1, uniform_buffer, 0, sizeof(glm::mat4));
        draw_mesh(model, model.meshes[node.mesh], cmd);
    }
    for (uint32_t i = 0; i < node.children.size(); i++) {
        draw_node(model, model.nodes[node.children[i]], cmd, local_to_world);
    }
}

void Application::draw_mesh(
    const tinygltf::Model& model,
    const tinygltf::Mesh& mesh,
    Morpho::Vulkan::CommandBuffer& cmd
) {
    for (auto& primitive : mesh.primitives) {
        draw_primitive(model, primitive, cmd);
    }
}

void Application::draw_primitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    Morpho::Vulkan::CommandBuffer& cmd
) {
    if (primitive.attributes.size() != 4) {
        return;
    }
    if (primitive.material < 0) {
        std::cout << "Primitive with no material" << std::endl;
        return;
    }
    uint32_t current_binding = 0;
    if (primitive.material != current_material_index) {
        auto& material = model.materials[primitive.material];
        VkVertexInputAttributeDescription attributes[Morpho::Vulkan::Limits::MAX_VERTEX_ATTRIBUTE_DESCRIPTION_COUNT];
        VkVertexInputBindingDescription bindings[Morpho::Vulkan::Limits::MAX_VERTEX_INPUT_BINDING_COUNT];
        uint32_t binding_count = 0;
        uint32_t attribute_count = 0;
        for (auto& key_value : primitive.attributes) {
            auto& attribute_name = key_value.first;
            auto accessor_index = key_value.second;
            auto& accessor = model.accessors[accessor_index];
            auto& buffer_view = model.bufferViews[accessor.bufferView];
            auto location = attribute_name_to_location.find(attribute_name);
            if (location == attribute_name_to_location.end()) {
                continue;
            }
            auto binding = attribute_name_to_binding.find(attribute_name)->second;
            attributes[location->second] = { current_binding, location->second, gltf_to_format(accessor.type, accessor.componentType), 0 };
            bindings[binding] = { binding, (uint32_t)accessor.ByteStride(buffer_view), VK_VERTEX_INPUT_RATE_VERTEX };
        }
        // Base color texture.
        auto base_color_texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
        auto base_color_image_view = base_color_texture_index < 0 ? white_image_view : texture_image_views[base_color_texture_index];
        auto base_color_sampler_index = base_color_texture_index < 0 ? -1 : model.textures[base_color_texture_index].sampler;
        auto base_color_sampler = base_color_sampler_index < 0 ? default_sampler : samplers[base_color_sampler_index];
        cmd.set_combined_image_sampler(1, 3, base_color_image_view, base_color_sampler);
        auto base_color_factor = glm::vec4(
            material.pbrMetallicRoughness.baseColorFactor[0],
            material.pbrMetallicRoughness.baseColorFactor[1],
            material.pbrMetallicRoughness.baseColorFactor[2],
            material.pbrMetallicRoughness.baseColorFactor[3]
        );
        auto base_color_factor_buffer = context->acquire_staging_buffer(
            sizeof(base_color_factor),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        base_color_factor_buffer.update(&base_color_factor, sizeof(base_color_factor));
        cmd.set_uniform_buffer(1, 0, base_color_factor_buffer, 0, sizeof(base_color_factor));
        // Normal map.
        auto normal_texture_index = material.normalTexture.index;
        auto normal_texture_image_view = normal_texture_index < 0 ? white_image_view : texture_image_views[normal_texture_index];
        auto normal_texture_sampler_index = normal_texture_index < 0 ? -1 : model.textures[normal_texture_index].sampler;
        auto normal_texture_sampler = normal_texture_sampler_index < 0 ? default_sampler : samplers[normal_texture_sampler_index];
        cmd.set_combined_image_sampler(1, 4, normal_texture_image_view, normal_texture_sampler);
        // TODO: if (prevDoubleSided != material.doubleSided => bindPipeline(ds ? pds : p)
        current_material_index = primitive.material;
    }
    current_binding = 0;
    for (auto& key_value : primitive.attributes) {
        if (attribute_name_to_location.find(key_value.first) == attribute_name_to_location.end()) {
            continue;
        }
        auto binding = attribute_name_to_binding.find(key_value.first)->second;
        auto& attribute_name = key_value.first;
        auto accessor_index = key_value.second;
        auto& accessor = model.accessors[accessor_index];
        auto& buffer_view = model.bufferViews[accessor.bufferView];
        cmd.bind_vertex_buffer(
            buffers[buffer_view.buffer],
            binding,
            accessor.byteOffset + buffer_view.byteOffset
        );
    }
    if (primitive.indices >= 0) {
        auto& accessor = model.accessors[primitive.indices];
        auto& buffer_view = model.bufferViews[accessor.bufferView];
        auto index_type = gltf_to_index_type(accessor.type, accessor.componentType);
        auto index_type_size = (uint32_t)tinygltf::GetComponentSizeInBytes(accessor.componentType);
        auto index_count = (uint32_t)accessor.count;
        cmd.bind_index_buffer(
            buffers[buffer_view.buffer],
            index_type,
            accessor.byteOffset + buffer_view.byteOffset
        );
        cmd.draw_indexed(
            index_count,
            1,
            0,
            0,
            0
        );
    }
}

struct hash_pair {
    template <class T1, class T2>
    size_t operator()(const std::pair<T1, T2>& p) const
    {
        auto first_hash = std::hash<T1>{}(p.first);
        auto second_hash = std::hash<T2>{}(p.second);
        return first_hash ^ second_hash;
    }
};

VkFormat Application::gltf_to_format(int type, int component_type) {
    static const std::unordered_map<std::pair<int, int>, VkFormat, hash_pair> map = {
        {{TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT}, VK_FORMAT_R32G32B32_SFLOAT},
        {{TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT}, VK_FORMAT_R32G32_SFLOAT},
        {{TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT}, VK_FORMAT_R32G32B32A32_SFLOAT},
    };
    return map.at(std::make_pair(type, component_type));
}

VkIndexType Application::gltf_to_index_type(int type, int component_type) {
    assert(type == TINYGLTF_TYPE_SCALAR);
    static const std::unordered_map<int, VkIndexType> map = {
        {TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, VK_INDEX_TYPE_UINT16},
        {TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, VK_INDEX_TYPE_UINT32},
    };
    return map.at(component_type);
}

VkFilter Application::gltf_to_filter(int filter) {
    static const std::unordered_map<int, VkFilter> map = {
        {TINYGLTF_TEXTURE_FILTER_NEAREST, VK_FILTER_NEAREST},
        {TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST, VK_FILTER_NEAREST},
        {TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR, VK_FILTER_NEAREST},
        {TINYGLTF_TEXTURE_FILTER_LINEAR, VK_FILTER_LINEAR},
        {TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST, VK_FILTER_LINEAR},
        {TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR, VK_FILTER_LINEAR},
    };
    return map.at(filter);
}

VkSamplerAddressMode Application::gltf_to_sampler_address_mode(int address_mode) {
    static const std::unordered_map<int, VkSamplerAddressMode> map = {
        {TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
        {TINYGLTF_TEXTURE_WRAP_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT},
        {TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT},
    };
    return map.at(address_mode);
}

void Application::setup_spot_light_uniforms(Morpho::Vulkan::CommandBuffer& cmd, const SpotLight& spot_light) {
    auto view_position = camera.get_position();
    auto view_buffer = context->acquire_staging_buffer(
        sizeof(view_position),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    view_buffer.update(&view_position, sizeof(view_position));
    cmd.set_uniform_buffer(1, 2, view_buffer, 0, sizeof(view_position));

    auto spot_light_buffer = context->acquire_staging_buffer(
        sizeof(spot_light),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    assert(spot_light.penumbra >= spot_light.umbra);
    spot_light_buffer.update(&spot_light, sizeof(spot_light));
    cmd.set_uniform_buffer(1, 1, spot_light_buffer, 0, sizeof(spot_light));
}

void Application::setup_point_light_uniforms(Morpho::Vulkan::CommandBuffer& cmd, const PointLight& point_light) {
    auto point_light_buffer = context->acquire_staging_buffer(
        sizeof(point_light),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    point_light_buffer.update(&point_light, sizeof(point_light));
    cmd.set_uniform_buffer(1, 1, point_light_buffer, 0, sizeof(point_light));
}

Morpho::Vulkan::Shader Application::load_shader(const std::string& path) {
    auto code = read_file(path);
    auto shader = context->acquire_shader(code.data(), (uint32_t)code.size());
    shader.set_stage(
        path.find(".vert") != std::string::npos
            ? Morpho::Vulkan::ShaderStage::VERTEX
            : Morpho::Vulkan::ShaderStage::FRAGMENT
    );
    shader.set_entry_point("main");
    return shader;
}
