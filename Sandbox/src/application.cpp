#include <bit>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/matrix.hpp>
#include <limits>
#include <stb_image.h>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "common/span.hpp"
#include "common/utils.hpp"
#include "math.hpp"
#include "vulkan/context.hpp"
#include "vulkan/resources.hpp"
#include "vulkan/resource_manager.hpp"
#include "application.hpp"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

void traverse_node(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    const glm::mat4& parent_to_world,
    FixedSizeAllocator* transforms,
    uint64_t alignment
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
        ModelUniform* uniform = (ModelUniform*)transforms->get_mapped_ptr(node.mesh);
        uniform->transform = local_to_world;
        uniform->inverse_transpose_transform = glm::transpose(glm::affineInverse(local_to_world));
    }
    for (const auto& child : node.children) {
        traverse_node(model, model.nodes[child], local_to_world, transforms, alignment);
    }
}

void precalculate_transforms(
    const tinygltf::Model& model,
    FixedSizeAllocator* transforms,
    uint64_t alignment
) {
    for (auto& scene : model.scenes) {
        for (auto& node : scene.nodes) {
            glm::mat4 local_to_world = glm::mat4(1.0f);
            traverse_node(model, model.nodes[node], local_to_world, transforms, alignment);
        }
    }
}

void Application::init() {
    resource_manager = Morpho::Vulkan::ResourceManager::get();
    Morpho::FramePool<Morpho::DrawStream*>::init(
        &draw_stream_pool,
        {
            .frames_in_flight_count = frame_in_flight_count,
            .user_data = nullptr,
            .add = [] (void* user_data) { return (Morpho::DrawStream*)calloc(1, sizeof(Morpho::DrawStream)); },
            .reset = [] (Morpho::DrawStream** ds, void* user_data) { (*ds)->reset(); },
        }
    );
    z_prepass_shader = load_shader("./assets/shaders/z_prepass.vert.spv");
    gltf_depth_pass_vertex_shader = load_shader("./assets/shaders/gltf_depth_pass.vert.spv");
    gltf_spot_light_vertex_shader = load_shader("./assets/shaders/gltf_spot_light.vert.spv");
    gltf_point_light_vertex_shader = load_shader("./assets/shaders/gltf_point_light.vert.spv");
    gltf_directional_light_vertex_shader = load_shader("./assets/shaders/gltf_directional_light.vert.spv");
    gltf_spot_light_fragment_shader = load_shader("./assets/shaders/gltf_spot_light.frag.spv");
    gltf_point_light_fragment_shader = load_shader("./assets/shaders/gltf_point_light.frag.spv");
    gltf_directional_light_fragment_shader = load_shader("./assets/shaders/gltf_directional_light.frag.spv");
    full_screen_triangle_shader = load_shader("./assets/shaders/full_screen_triangle.vert.spv");
    shadow_map_spot_light_fragment_shader = load_shader("./assets/shaders/shadow_map_spot_light.frag.spv");
    no_light_vertex_shader = load_shader("./assets/shaders/no_light.vert.spv");
    no_light_fragment_shader = load_shader("./assets/shaders/no_light.frag.spv");

    using namespace Morpho::Vulkan;

    color_pass_layout = resource_manager->create_render_pass_layout(RenderPassLayoutInfoBuilder()
        .attachment(depth_format)
        .attachment(context->get_swapchain_format())
        .subpass({1}, 0)
        .info()
    );

    depth_pass_layout = resource_manager->create_render_pass_layout(RenderPassLayoutInfoBuilder()
        .attachment(depth_format)
        .subpass({}, 0)
        .info()
    );

    depth_pass = resource_manager->create_render_pass(RenderPassInfoBuilder()
        .layout(depth_pass_layout)
        .attachment(
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ).info()
    );

    color_pass = resource_manager->create_render_pass(Morpho::Vulkan::RenderPassInfoBuilder()
        .layout(color_pass_layout)
        .attachment(
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ).attachment(
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        ).info()
    );
    imgui_pass = resource_manager->create_render_pass(Morpho::Vulkan::RenderPassInfoBuilder()
        .layout(color_pass_layout)
        .attachment(
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        ).attachment(
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        ).info()
    );

    // globals
    VkDescriptorSetLayoutBinding set0_bindings[4] = {};
    // light
    VkDescriptorSetLayoutBinding set1_bindings[3] = {};
    // material
    VkDescriptorSetLayoutBinding set2_bindings[4] = {};
    // object
    VkDescriptorSetLayoutBinding set3_bindings[1] = {};
    PipelineLayoutInfo pipeline_layout_info{};
    pipeline_layout_info.set_binding_infos[0] = set0_bindings;
    pipeline_layout_info.set_binding_infos[1] = set1_bindings;
    pipeline_layout_info.set_binding_infos[2] = set2_bindings;
    pipeline_layout_info.set_binding_infos[3] = set3_bindings;
    {
        VkShaderStageFlags vertex_and_fragment = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_layout_info.set_binding_count[0] = 2;
        pipeline_layout_info.max_descriptor_set_counts[0] = frame_in_flight_count;
        // View, Projection.
        set0_bindings[0] = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, vertex_and_fragment, };
        // Camera position.
        set0_bindings[1] = { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, vertex_and_fragment, };
        pipeline_layout_info.set_binding_count[1] = 3;
        // TODO: there should be a better way..
        const uint32_t cube_texture_array_count = 6;
        // + frame_in_flight_count for debug DS and for cascaded shadow map.
        pipeline_layout_info.max_descriptor_set_counts[1] = max_light_count * frame_in_flight_count
            * (1 + cube_texture_array_count) + frame_in_flight_count + frame_in_flight_count * (cascade_count + 1);
        // Light's View and Projection.
        set1_bindings[0] = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, vertex_and_fragment, };
        // Light structure.
        set1_bindings[1] = { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, vertex_and_fragment, };
        // Shadow map.
        set1_bindings[2] = { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, };
        pipeline_layout_info.set_binding_count[2] = 4;
        pipeline_layout_info.max_descriptor_set_counts[2] = model.materials.size();
        // Material data.
        set2_bindings[0] = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, vertex_and_fragment, };
        // Base color texture.
        set2_bindings[1] = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, };
        // Normal map.
        set2_bindings[2] = { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, };
        // Metallness-roughness texture.
        set2_bindings[3] = { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, };
        pipeline_layout_info.set_binding_count[3] = 1;
        pipeline_layout_info.max_descriptor_set_counts[3] = model.meshes.size();
        set3_bindings[0] = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, vertex_and_fragment, };
        light_pipeline_layout = resource_manager->create_pipeline_layout(pipeline_layout_info);
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
    Morpho::Handle<Shader> shaders[2];
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
    pipeline_info.blend_state = additive;
    pipeline_info.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipeline_info.render_pass_layout = color_pass_layout;
    pipeline_info.pipeline_layout = light_pipeline_layout;
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
        spotlight_pipeline = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        spotlight_pipeline_double_sided = resource_manager->create_pipeline(pipeline_info);
    }
    {
        // Point light shading.
        pipeline_info.shaders[0] = gltf_point_light_vertex_shader;
        pipeline_info.shaders[1] = gltf_point_light_fragment_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        pointlight_pipeline = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        pointlight_pipeline_double_sided = resource_manager->create_pipeline(pipeline_info);
    }
    {
        // Directional light shading.
        pipeline_info.shaders[0] = gltf_directional_light_vertex_shader;
        pipeline_info.shaders[1] = gltf_directional_light_fragment_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        directional_light_pipeline = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        directional_light_pipeline_double_sided = resource_manager->create_pipeline(pipeline_info);
    }
    pipeline_info.blend_state = no_blend;
    {
        // No light shading.
        pipeline_info.shaders[0] = no_light_vertex_shader;
        pipeline_info.shaders[1] = no_light_fragment_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        no_light_pipeline = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        no_light_pipeline_double_sided = resource_manager->create_pipeline(pipeline_info);
    }
    {
        // Z prepass.
        pipeline_info.shader_count = 1;
        pipeline_info.shaders[0] = z_prepass_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        z_prepass_pipeline = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        z_prepass_pipeline_double_sided = resource_manager->create_pipeline(pipeline_info);
    }
    {
        // Depth pass.
        pipeline_info.depth_bias_constant_factor = 5.0f;
        pipeline_info.depth_bias_slope_factor = 3.0;
        pipeline_info.shader_count = 1;
        pipeline_info.shaders[0] = gltf_depth_pass_vertex_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        pipeline_info.render_pass_layout = depth_pass_layout;
        depth_pass_pipeline_ccw = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.depth_clamp_enabled = true;
        depth_pass_pipeline_ccw_depth_clamp = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        depth_pass_pipeline_ccw_depth_clamp_double_sided = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.depth_clamp_enabled = false;
        depth_pass_pipeline_ccw_double_sided = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.front_face = VK_FRONT_FACE_CLOCKWISE;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        depth_pass_pipeline_cw = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.cull_mode = VK_CULL_MODE_NONE;
        depth_pass_pipeline_cw_double_sided = resource_manager->create_pipeline(pipeline_info);
        pipeline_info.depth_clamp_enabled = false;
    }

    pipeline_info.depth_test_enabled = false;
    pipeline_info.depth_write_enabled = false;
    pipeline_info.depth_bias_constant_factor = pipeline_info.depth_bias_slope_factor = 0.0f;
    {
        // Shadow map debug
        pipeline_info.attribute_count = 0;
        pipeline_info.binding_count = 0;
        pipeline_info.render_pass_layout = color_pass_layout;
        pipeline_info.shader_count = 2;
        pipeline_info.shaders[0] = full_screen_triangle_shader;
        pipeline_info.shaders[1] = shadow_map_spot_light_fragment_shader;
        pipeline_info.cull_mode = VK_CULL_MODE_BACK_BIT;
        pipeline_info.pipeline_layout = light_pipeline_layout;
        shadow_map_visualization_pipeline = resource_manager->create_pipeline(pipeline_info);
    }

    default_sampler = resource_manager->create_sampler({ .max_anisotropy = 4.0f, });
    shadow_sampler = resource_manager->create_sampler({
        .compare_enable = VK_TRUE,
        .compare_op = VK_COMPARE_OP_LESS,
        .max_anisotropy = 4.0f,
    });

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
        buffers[i] = resource_manager->create_buffer({
            .size = buffer_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | buffer_usages[i],
            .initial_data = model.buffers[i].data.data(),
            .initial_data_size = buffer_size
        });
    }
    uint32_t white_pixel = std::numeric_limits<uint32_t>::max();
    white_texture = resource_manager->create_texture({
        .extent = { 1, 1, 1 },
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .image_usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initial_data = &white_pixel,
        .initial_data_size = sizeof(white_pixel)
    });

    std::vector<VkFormat> texture_formats(model.textures.size(), VK_FORMAT_UNDEFINED);
    // determine texture formats cause:
    // 1. stb_image / tinygltf doesn't provide any info on color space
    // 2. color space is *always* set to wrong one and, according to the spec, needs to be ignored (w t f)
    for (uint32_t material_index = 0; material_index < model.materials.size(); material_index++) {
        auto& material = model.materials[material_index];
        auto base_color_texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
        auto normal_texture_index = material.normalTexture.index;
        auto metalic_roughness_texture_index = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (metalic_roughness_texture_index >= 0) {
            assert(
                texture_formats[metalic_roughness_texture_index] == VK_FORMAT_UNDEFINED
                || texture_formats[metalic_roughness_texture_index] == VK_FORMAT_R8G8B8A8_UNORM
            );
            texture_formats[metalic_roughness_texture_index] = VK_FORMAT_R8G8B8A8_UNORM;
        }
        if (normal_texture_index >= 0) {
            assert(
                texture_formats[normal_texture_index] == VK_FORMAT_UNDEFINED
                || texture_formats[normal_texture_index] == VK_FORMAT_R8G8B8A8_UNORM
            );
            texture_formats[normal_texture_index] = VK_FORMAT_R8G8B8A8_UNORM;
        }
        if (base_color_texture_index >= 0) {
            assert(
                texture_formats[base_color_texture_index] == VK_FORMAT_UNDEFINED
                || texture_formats[base_color_texture_index] == VK_FORMAT_R8G8B8A8_SRGB
            );
            texture_formats[base_color_texture_index] = VK_FORMAT_R8G8B8A8_SRGB;
        }
    }

    textures.resize(model.textures.size());
    for (uint32_t i = 0; i < model.textures.size(); i++) {
        auto& texture = model.textures[i];
        auto gltf_image = model.images[texture.source];
        assert(gltf_image.component == 4);
        auto image_size = (VkDeviceSize)(gltf_image.width * gltf_image.height * gltf_image.component * (gltf_image.bits / 8));
        VkFormat format = texture_formats[i];
        uint32_t mip_level_count = std::bit_width((uint32_t)std::max(gltf_image.width, gltf_image.height));
        textures[i] = resource_manager->create_texture({
            .extent = { (uint32_t)gltf_image.width, (uint32_t)gltf_image.height, (uint32_t)1 },
            .format = format,
            .image_usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .mip_level_count = mip_level_count,
            .initial_data = gltf_image.image.data(),
            .initial_data_size = image_size,
            .initial_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        });
    }
    samplers.resize(model.samplers.size());
    for (uint32_t i = 0; i < model.samplers.size(); i++) {
        auto gltf_sampler = model.samplers[i];
        samplers[i] = resource_manager->create_sampler({
            .address_mode_u = gltf_to_sampler_address_mode(gltf_sampler.wrapT),
            .address_mode_v = gltf_to_sampler_address_mode(gltf_sampler.wrapS),
            .address_mode_w = gltf_to_sampler_address_mode(gltf_sampler.wrapT),
            .min_filter = gltf_to_filter(gltf_sampler.minFilter),
            .mag_filter = gltf_to_filter(gltf_sampler.magFilter),
            .max_anisotropy = 4.0f,
        });
    }
    const uint64_t alignment = context->get_uniform_buffer_alignment();
    material_descriptor_sets.resize(model.materials.size());
    material_buffer = resource_manager->create_buffer({
        .size = FixedSizeAllocator::compute_buffer_size(sizeof(MaterialParameters), model.materials.size(), alignment),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .map = Morpho::Vulkan::BufferMap::PERSISTENTLY_MAPPED,
    });
    material_buffer_allocator = FixedSizeAllocator::create({
        .resource_manager = resource_manager,
        .buffer = material_buffer,
        .item_size = sizeof(MaterialParameters),
        .offset_alignment = alignment,
        .max_item_count = model.materials.size(),
    });
    for (uint32_t material_index = 0; material_index < model.materials.size(); material_index++) {
        auto& material = model.materials[material_index];
        material_descriptor_sets[material_index] = resource_manager->create_descriptor_set(light_pipeline_layout, 2);
        uint64_t offset = material_buffer_allocator.get_offset(material_index);
        auto base_color_texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
        auto base_color_texture = base_color_texture_index < 0
            ? white_texture : textures[base_color_texture_index];
        auto base_color_sampler_index = base_color_texture_index < 0
            ? -1 : model.textures[base_color_texture_index].sampler;
        auto base_color_sampler = base_color_sampler_index < 0 ? default_sampler : samplers[base_color_sampler_index];
        auto normal_texture_index = material.normalTexture.index;
        auto normal_texture = normal_texture_index < 0 ? white_texture : textures[normal_texture_index];
        auto normal_texture_sampler_index = normal_texture_index < 0
            ? -1 : model.textures[normal_texture_index].sampler;
        auto normal_texture_sampler = normal_texture_sampler_index < 0
            ? default_sampler : samplers[normal_texture_sampler_index];
        auto metalic_roughness_texture_index = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        auto metalic_roughness_texture = metalic_roughness_texture_index < 0
            ? white_texture : textures[metalic_roughness_texture_index];
        auto metalic_roughness_texture_sampler_index = metalic_roughness_texture_index < 0
            ? -1 : model.textures[metalic_roughness_texture_index].sampler;
        auto metalic_roughness_texture_sampler = metalic_roughness_texture_index < 0
            ? default_sampler : samplers[metalic_roughness_texture_sampler_index];
        resource_manager->update_descriptor_set(
            material_descriptor_sets[material_index],
            {
                {
                    .binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .buffer_infos = {{ material_buffer, offset, sizeof(MaterialParameters), }}
                },
                {
                    .binding = 1, .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .texture_infos = {{ base_color_texture, base_color_sampler, }}
                },
                {
                    .binding = 2, .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .texture_infos = {{ normal_texture, normal_texture_sampler, }}
                },
                {
                    .binding = 3, .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .texture_infos = {{ metalic_roughness_texture, metalic_roughness_texture_sampler, }}
                }
            }
        );
        auto base_color_factor = glm::vec4(
            material.pbrMetallicRoughness.baseColorFactor[0],
            material.pbrMetallicRoughness.baseColorFactor[1],
            material.pbrMetallicRoughness.baseColorFactor[2],
            material.pbrMetallicRoughness.baseColorFactor[3]
        );
        MaterialParameters material_parameters{};
        material_parameters.base_color_factor = base_color_factor;
        material_parameters.metalness_factor = material.pbrMetallicRoughness.metallicFactor;
        material_parameters.roughness_factor = material.pbrMetallicRoughness.roughnessFactor;
        memcpy(material_buffer_allocator.get_mapped_ptr(material_index), &material_parameters, sizeof(material_parameters));
    }

    globals_buffer = resource_manager->create_buffer({
        .size = FixedSizeAllocator::compute_buffer_size(sizeof(Globals), frame_in_flight_count, alignment),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .map = Morpho::Vulkan::BufferMap::PERSISTENTLY_MAPPED,
    });
    globals_allocator = FixedSizeAllocator::create({
        .resource_manager = resource_manager,
        .buffer = globals_buffer,
        .item_size = sizeof(Globals),
        .offset_alignment = alignment,
        .max_item_count = frame_in_flight_count,
    });
    for (uint32_t i = 0; i < frame_in_flight_count; i++) {
        global_descriptor_sets[i] = resource_manager->create_descriptor_set(light_pipeline_layout, 0);
        resource_manager->update_descriptor_set(
            global_descriptor_sets[i],
            {
                {
                    .binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .buffer_infos = {{ globals_buffer, globals_allocator.get_offset(i), sizeof(Globals), }}
                },
            }
        );
    }
    for (uint32_t i = 0; i < frame_in_flight_count; i++) {
        shadow_map_visualization_descriptor_set[i] = resource_manager->create_descriptor_set(light_pipeline_layout, 1);
    }

    mesh_descriptor_sets.resize(model.meshes.size());
    
    mesh_uniforms = resource_manager->create_buffer({
        .size = FixedSizeAllocator::compute_buffer_size(sizeof(ModelUniform), model.meshes.size(), alignment),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .map = BufferMap::CAN_BE_MAPPED,
    });
    mesh_uniforms_allocator = FixedSizeAllocator::create({
        .resource_manager = resource_manager,
        .buffer = mesh_uniforms,
        .item_size = sizeof(ModelUniform),
        .offset_alignment = alignment,
        .max_item_count = model.meshes.size()
    });
    precalculate_transforms(model, &mesh_uniforms_allocator, alignment);
    for (uint32_t mesh_index = 0; mesh_index < model.meshes.size(); mesh_index++) {
        mesh_descriptor_sets[mesh_index] = resource_manager->create_descriptor_set(light_pipeline_layout, 3);
        uint64_t offset = mesh_index * Morpho::align_up_pow2(sizeof(ModelUniform), alignment);
        resource_manager->update_descriptor_set(
            mesh_descriptor_sets[mesh_index],
            {
                {
                    .binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .buffer_infos = {{ mesh_uniforms, offset, sizeof(ModelUniform), }}
                },
            }
        );
    }
    auto extent = context->get_swapchain_extent();
    depth_buffer = resource_manager->create_texture({
        .extent = { extent.width, extent.height, 1 },
        .format = depth_format,
        .image_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    });
    uint32_t max_side_length = std::max(extent.width, extent.height);
    cascaded_shadow_maps = resource_manager->create_texture({
        .extent = { max_side_length, max_side_length, 1 },
        .format = depth_format,
        .image_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .array_layer_count = cascade_count,
        .initial_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    });
    for (uint32_t frame_index = 0; frame_index < frame_in_flight_count; frame_index++) {
        csm_descriptor_sets[frame_index] = resource_manager->create_descriptor_set(light_pipeline_layout, 1);
        resource_manager->update_descriptor_set(
            csm_descriptor_sets[frame_index],
            {
                {
                    .binding = 2, .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .texture_infos = {{ cascaded_shadow_maps, shadow_sampler, }},
                },
            }
        );
    }
    for (uint32_t i = 0; i < cascade_count; i++) {
        directional_shadow_maps[i] = resource_manager->create_texture_view(cascaded_shadow_maps, i, 1);
    }
    for (uint32_t cascade_index = 0; cascade_index < cascade_count; cascade_index++) {
        for (uint32_t frame_index = 0; frame_index < frame_in_flight_count; frame_index++) {
            uint32_t ds_index = cascade_index * frame_in_flight_count + frame_index;
            directional_shadow_map_descriptor_sets[ds_index] = resource_manager->create_descriptor_set(light_pipeline_layout, 1);
            resource_manager->update_descriptor_set(
                directional_shadow_map_descriptor_sets[ds_index],
                {
                    {
                        .binding = 2, .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .texture_infos = {{ directional_shadow_maps[cascade_index], shadow_sampler, }},
                    },
                }
            );
        }
    }
    UniformBufferBumpAllocator::init(
        {
            .resource_manager = resource_manager,
            .alignment = alignment,
            .frames_in_flight_count = frame_in_flight_count,
        },
        &per_frame_uniforms
    );
}

void Application::run() {
    init_window();
    initialize_key_map();
    context->init(window);
    context->set_frame_context_count(frame_in_flight_count);
    auto swapchain_extent = context->get_swapchain_extent();
    camera = Camera(
        90.0f,
        0.0f,
        glm::vec3(0, 1, 0),
        glm::radians(60.0f),
        swapchain_extent.width / (float)swapchain_extent.height,
        0.01f,
        100.0f
    );
    camera.set_position(glm::vec3(0.0f, 0.0f, 0.0f));
    init();
    init_imgui();
    main_loop();
}

void Application::init_window() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(window_width, window_height, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, process_keyboard_input);
    glfwSetCursorPosCallback(window, process_cursor_position);
    glfwSetMouseButtonCallback(window, process_mouse_button_input);
}

void Application::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    context->get_vulkans_guts(&init_info.Instance, &init_info.PhysicalDevice, &init_info.Device, &init_info.Queue, &init_info.QueueFamily, &init_info.DescriptorPool);
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.RenderPass = resource_manager->get_render_pass(color_pass).render_pass;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&init_info);
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
        gui((float)delta);
        render_frame();
    }
}

void Application::begin_frame(Morpho::Vulkan::CommandBuffer* cmd) {
    // Transition all RTs to layout required in the beginning of the frame.
    texture_barriers.push_back({
       .texture = context->get_swapchain_texture(),
       .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
       .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       .src_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
       .src_access = 0,
       .dst_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
       .dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
    });
    texture_barriers.push_back({
        .texture = depth_buffer,
        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .src_stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .src_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dst_stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    });
    texture_barriers.push_back({
        .texture = cascaded_shadow_maps,
        .old_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .src_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .src_access = VK_ACCESS_SHADER_READ_BIT,
        .dst_stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    });
    for (uint32_t i = 0; i < lights.size(); i++) {
        const Light& light = lights[i];
        texture_barriers.push_back({
            .texture = light.shadow_map,
            .old_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .src_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .src_access = VK_ACCESS_SHADER_READ_BIT,
            .dst_stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dst_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        });
    }
    cmd->barrier(
        Morpho::make_const_span(texture_barriers.data(), texture_barriers.size()),
        {}
    );
    texture_barriers.clear();
}

void Application::transition_shadow_maps(Morpho::Vulkan::CommandBuffer* cmd) {
   texture_barriers.push_back({
       .texture = cascaded_shadow_maps,
       .old_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
       .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
       .src_stages = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
       .src_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
       .dst_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
       .dst_access = VK_ACCESS_SHADER_READ_BIT,
    });
    for (uint32_t i = 0; i < lights.size(); i++) {
        const Light& light = lights[i];
        texture_barriers.push_back({
            .texture = light.shadow_map,
            .old_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            .src_stages = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .src_access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dst_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dst_access = VK_ACCESS_SHADER_READ_BIT,
        });
    }
    cmd->barrier(
        Morpho::make_const_span(texture_barriers.data(), texture_barriers.size()),
        {}
    );
    texture_barriers.clear();
}

void Application::set_graphics_context(Morpho::Vulkan::Context* context) {
    this->context = context;
}

void Application::render_frame() {
    context->begin_frame();
    resource_manager->next_frame();
    draw_stream_pool.next_frame();
    per_frame_uniforms.next_frame();
    calculate_cascades();
    update_light_uniforms();
    Morpho::Vulkan::CommandBuffer* cmd = context->acquire_command_buffer();
    if (is_first_update) {
        initialize_static_resources(cmd);
        is_first_update = false;
    }
    begin_frame(cmd);
    cmd->bind_descriptor_set(global_descriptor_sets[frame_index]);
    render_depth_pass_for_directional_light(cmd);
    for (uint32_t i = 0; i < lights.size(); i++) {
        if (lights[i].light_type == LightType::SpotLight) {
            render_depth_pass_for_spot_light(cmd, lights[i]);
        }
    }
    transition_shadow_maps(cmd);
    Morpho::DrawStream* stream = draw_stream_pool.get_or_add();
    render_z_prepass(stream);
    render_color_pass_for_directional_light(stream);
    for (int i = 0; i < lights.size(); i++) {
        if (lights[i].light_type == LightType::SpotLight) {
            render_color_pass_for_spotlight(stream, lights[i]);
        }
    }
    auto extent = context->get_swapchain_extent();
    auto framebuffer = context->acquire_framebuffer(Morpho::Vulkan::FramebufferInfoBuilder()
        .layout(color_pass_layout)
        .extent(extent)
        .attachment(depth_buffer)
        .attachment(context->get_swapchain_texture())
        .info()
    );
    cmd->decode_stream({
        .render_pass = color_pass,
        .framebuffer = framebuffer,
        .render_area = { .offset = { 0, 0 }, .extent = extent },
        .global_ds = global_descriptor_sets[frame_index],
        .clear_values = {
            {1.0f, 0},
            {0.0f, 0.0f, 0.0f, 0.0f},
        },
        .stream = Morpho::make_const_span(stream->get_stream(), stream->get_size()),
    });
    render_gui(cmd);
    cmd->barrier(
        {{
           .texture = context->get_swapchain_texture(),
           .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
           .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
           .src_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
           .src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
           .dst_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
           .dst_access = VK_ACCESS_MEMORY_READ_BIT,
        }},
        {}
    );
    resource_manager->commit();
    context->submit(cmd);
    context->end_frame();
    frames_total++;
    frame_index = frames_total % frame_in_flight_count;
}

void Application::update_light_uniforms() {
    for (uint32_t light_index = 0; light_index < lights.size(); light_index++) {
        ViewProjection vp;
        VkExtent2D extent = context->get_swapchain_extent();
        UniformAllocation light_data_allocation{};
        if (lights[light_index].light_type == LightType::PointLight) {
            extent.width = extent.height = std::max(extent.width, extent.height);
            vp.view = glm::translate(glm::mat4(1.0f), -lights[light_index].light_data.point_light.position);
        } else if (lights[light_index].light_type == LightType::SpotLight) {
            auto& spot_light = lights[light_index].light_data.spot_light;
            glm::vec3 forward = -glm::normalize(spot_light.direction);
            glm::vec3 right = glm::normalize(glm::cross(world_up, forward));
            glm::vec3 up = glm::cross(forward, right);
            glm::vec3 pos = spot_light.position;
            vp.view = glm::mat4(
                right.x, -up.x, -forward.x, 0.0f,
                right.y, -up.y, -forward.y, 0.0f,
                right.z, -up.z, -forward.z, 0.0f,
                -dot(right, pos), -dot(-up, pos), -dot(-forward, pos), 1.0f
            );
        } else {
            assert(false);
        }
        vp.proj = perspective(glm::radians(90.0f), extent.width / (float)extent.height, 0.01f, 100.0f);
        UniformAllocation vp_allocation = per_frame_uniforms.allocate(sizeof(vp));
        memcpy(vp_allocation.ptr, &vp, sizeof(vp));
        light_data_allocation = per_frame_uniforms.allocate(sizeof(Light::LightData));
        memcpy(light_data_allocation.ptr, &lights[light_index].light_data, sizeof(Light::LightData));

        resource_manager->update_descriptor_set(
            light_descriptor_sets[lights[light_index].descriptor_set_start_index + frame_index],
            {
                {
                    .binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .buffer_infos = {{ vp_allocation.buffer, vp_allocation.offset, sizeof(ViewProjection), }},
                },
                {
                    .binding = 1, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .buffer_infos = {{ light_data_allocation.buffer, light_data_allocation.offset, sizeof(Light::LightData), }},
                },
            }
        );
    }
}

void Application::render_depth_pass_for_spot_light(
    Morpho::Vulkan::CommandBuffer* cmd,
    const Light& light
) {
    auto extent = context->get_swapchain_extent();
    Morpho::DrawStream* draw_stream = draw_stream_pool.get_or_add();
    auto framebuffer = context->acquire_framebuffer(Morpho::Vulkan::FramebufferInfoBuilder()
        .layout(depth_pass_layout)
        .extent(extent)
        .attachment(light.shadow_map)
        .info()
    );
    draw_stream->bind_descriptor_set(light_descriptor_sets[light.descriptor_set_start_index + frame_index], 1);
    draw_model(model, draw_stream, depth_pass_pipeline_ccw, depth_pass_pipeline_ccw_double_sided);
    cmd->decode_stream({
        .render_pass = depth_pass,
        .framebuffer = framebuffer,
        .render_area = { .offset = { 0, 0 }, .extent = extent },
        .global_ds = global_descriptor_sets[frame_index],
        .clear_values = {
            {1.0f, 0},
            {0.0f, 0.0f, 0.0f, 0.0f},
        },
        .stream = Morpho::make_const_span(draw_stream->get_stream(), draw_stream->get_size()),
    });
}

void Application::render_depth_pass_for_directional_light(Morpho::Vulkan::CommandBuffer* cmd) {
   auto extent = context->get_swapchain_extent();
   extent.width = extent.height = std::max(extent.width, extent.height);
   cmd->set_viewport({ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f });
   cmd->set_scissor({ {0, 0}, extent });
   for (uint32_t cascade_index = 0; cascade_index < cascade_count; cascade_index++) {
        auto framebuffer = context->acquire_framebuffer(Morpho::Vulkan::FramebufferInfoBuilder()
           .layout(depth_pass_layout)
           .extent(extent)
           .attachment(directional_shadow_maps[cascade_index])
           .info()
        );
        Morpho::DrawStream* draw_stream = draw_stream_pool.get_or_add();
        draw_stream->bind_descriptor_set(directional_shadow_map_descriptor_sets[cascade_index * frame_in_flight_count + frame_index], 1);
        draw_model(model, draw_stream, depth_pass_pipeline_ccw_depth_clamp, depth_pass_pipeline_ccw_depth_clamp_double_sided);
        cmd->decode_stream({
            .render_pass = depth_pass,
            .framebuffer = framebuffer,
            .render_area = { .offset = { .x = 0, .y = 0 }, .extent = extent },
            .global_ds = global_descriptor_sets[frame_index],
            .clear_values = { { 1.0f, 0}, {0.0f, 0.0f, 0.0f, 0.0f} },
            .stream = Morpho::make_const_span(draw_stream->get_stream(), draw_stream->get_size()),
        });
   }
}

void Application::render_z_prepass(Morpho::DrawStream* draw_stream) {
    draw_model(model, draw_stream, z_prepass_pipeline, z_prepass_pipeline_double_sided);
}

void Application::render_color_pass_for_directional_light(Morpho::DrawStream* stream) {
    stream->bind_descriptor_set(csm_descriptor_sets[frame_index], 1);
    draw_model(model, stream, directional_light_pipeline, directional_light_pipeline_double_sided);
}

void Application::render_color_pass_for_spotlight(
    Morpho::DrawStream* stream,
    const Light& light
) {
    stream->bind_descriptor_set(light_descriptor_sets[light.descriptor_set_start_index + frame_index], 1);
    draw_model(model, stream, spotlight_pipeline, spotlight_pipeline_double_sided);
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

void Application::initialize_static_resources(Morpho::Vulkan::CommandBuffer* cmd) {
    generate_mipmaps(cmd);
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

void Application::process_mouse_button_input(GLFWwindow* window, int button, int action, int mods) {
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

void Application::add_light(Light light) {
    light.descriptor_set_start_index = light_descriptor_sets.size();
    for (uint32_t i = 0; i < frame_in_flight_count; i++) {
        auto ds = resource_manager->create_descriptor_set(light_pipeline_layout, 1);
        resource_manager->update_descriptor_set(
            ds,
            {
                {
                    .binding = 2, .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .texture_infos = {{ light.shadow_map, shadow_sampler }},
                },
            }
        );
        light_descriptor_sets.push_back(ds);
    }
    if (light.light_type == LightType::PointLight) {
        for (uint32_t face_index = 0; face_index < 6; face_index++) {
            light.views[face_index] = resource_manager->create_texture_view(
                light.shadow_map,
                face_index,
                1
            );
        }
        // TODO: just decouple spot and point lights into different arrays.
        for (uint32_t i = 0; i < frame_in_flight_count; i++) {
            for (uint32_t face_index = 0; face_index < 6; face_index++) {
                auto ds = resource_manager->create_descriptor_set(light_pipeline_layout, 1);
                resource_manager->update_descriptor_set(
                    ds,
                    {
                        {
                            .binding = 2, .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .texture_infos = {{ light.views[face_index], shadow_sampler }},
                        },
                    }
                );
                cube_map_face_descriptor_sets.push_back(ds);
            }
        }
    }
    lights.push_back(light);
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

    if (input.was_key_pressed(Key::M)) {
        debug_mode = !debug_mode;
    }

    bool up_pressed = input.is_key_pressed(Key::UP);
    bool down_pressed = input.is_key_pressed(Key::DOWN);
    bool right_pressed = input.is_key_pressed(Key::RIGHT);
    bool left_pressed = input.is_key_pressed(Key::LEFT);
    if (debug_mode) {
        if (input.was_key_pressed(Key::UP)) {
            current_slice_index = (current_slice_index + 1) % cascade_count;
        } else if (input.was_key_pressed(Key::DOWN)) {
            current_slice_index = (current_slice_index + cascade_count - 1) % cascade_count;
        }
    } else {
        int32_t change_x = (left_pressed ^ right_pressed) * (right_pressed ? 1 : -1);
        int32_t change_z = (up_pressed ^ down_pressed) * (up_pressed ? 1 : -1);
        if (change_x != 0 || change_z != 0) {
            sun.direction.x += change_x * 0.1 * delta;
            sun.direction.z += change_z * 0.1 * delta;
            sun.direction = glm::normalize(sun.direction);
        }
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
    auto extent = context->get_swapchain_extent();
    if (input.was_key_pressed(Key::F)) {
        assert(this->light_descriptor_sets.size() < max_light_count);
        SpotLight light_data = SpotLight(
            camera.get_position(),
            camera.get_forward(),
            glm::vec3(1.0f, 1.0f, 1.0f),
            14.0f,
            3.0f,
            cos(glm::radians(17.5f)),
            cos(glm::radians(12.5f))
        );
        auto shadow_map = resource_manager->create_texture({
            .extent = { extent.width, extent.height, 1 },
            .format = depth_format,
            .image_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initial_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        });
        Light light{};
        light.light_type = LightType::SpotLight;
        light.light_data.spot_light = light_data;
        light.shadow_map = shadow_map;
        add_light(light);
    }
    Globals globals;
    globals.view = camera.get_view();
    globals.proj = camera.get_projection();
    globals.camera_position = camera.get_position();
    memcpy(globals_allocator.get_mapped_ptr(frame_index), &globals, sizeof(Globals));
}

void Application::gui(float delta) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    static bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);
    ImGui::Render();
}

void Application::render_gui(Morpho::Vulkan::CommandBuffer* cmd) {
    ImDrawData* draw_data = ImGui::GetDrawData();
    auto extent = context->get_swapchain_extent();
    auto framebuffer = context->acquire_framebuffer(Morpho::Vulkan::FramebufferInfoBuilder()
        .layout(color_pass_layout)
        .extent(extent)
        .attachment(depth_buffer)
        .attachment(context->get_swapchain_texture())
        .info()
    );
    cmd->begin_render_pass(
        imgui_pass,
        framebuffer,
        { .offset = { 0, 0 }, .extent = extent },
        { }
    );
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmd->get_vulkan_handle());
    cmd->end_render_pass();
}

void Application::calculate_cascades() {
    UniformAllocation dir_light_alloc = per_frame_uniforms.allocate(sizeof(DirectionalLight));
    memcpy(dir_light_alloc.ptr, &sun, sizeof(sun));

    glm::mat4 camera_to_world = camera.get_transform();
    glm::vec3 forward = -sun.direction;
    glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), forward));
    glm::vec3 up = glm::cross(forward, right);
    glm::mat4 light_to_world = glm::mat4(
        right.x,    right.y,    right.z,   0.0f,
       -up.x,      -up.y,      -up.z,      0.0f,
       -forward.x, -forward.y, -forward.z, 0.0f,
        0.0f,       0.0f,       0.0f,      1.0f
    );
    glm::mat4 world_to_light = glm::mat4(
        right.x, -up.x, -forward.x, 0.0f,
        right.y, -up.y, -forward.y, 0.0f,
        right.z, -up.z, -forward.z, 0.0f,
        0.0f,     0.0f,  0.0f,      1.0f
    );
    auto extent = context->get_swapchain_extent();
    extent.width = extent.height = std::max(extent.width, extent.height);
    float near = camera.get_near();
    float far = camera.get_far();
    float split_lamda = 0.10;
    CsmUniform csm_uniform{};
    glm::vec3 first_cascade_position;
    float first_cascade_side_length;
    glm::vec2 first_cascade_z_range;
    glm::mat4 camera_to_light = world_to_light * camera_to_world;
    for (uint32_t cascade_index = 0; cascade_index < cascade_count; cascade_index++) {
        glm::vec2 range_uniform = glm::vec2(
            lerp(near, far, (float)cascade_index / cascade_count),
            lerp(near, far, (float)(cascade_index + 1) / cascade_count)
        );
        glm::vec2 range_log = glm::vec2(
            near * std::powf(far / near, (float)cascade_index / cascade_count),
            near * std::powf(far / near, (float)(cascade_index + 1) / cascade_count)
        );
        glm::vec2 range_practical = glm::vec2(
            lerp(range_log.x, range_uniform.x, split_lamda),
            lerp(range_log.y, range_uniform.y, split_lamda)
        );
        glm::vec2 range = range_practical;
        if (cascade_index != 0) {
            // so that they overlap a bit to blend between cascades:
            range.x = std::clamp(range.x - 1.0f, near, far);
        }
        Frustum frustum = camera.get_frustum(range.x, range.y);
        float max_side_length = std::ceil(std::max(
                glm::length(frustum[0] - frustum[6]),
                glm::length(frustum[4] - frustum[6])
        ));
        glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());
        for (uint32_t i = 0; i < 8; i++) {
            glm::vec3 light_space = camera_to_light * glm::vec4(frustum[i], 1.0);
            min = glm::min(min, light_space);
            max = glm::max(max, light_space);
        }
        ViewProjection vp;
        vp.proj = ortho(max_side_length, max_side_length, max.z - min.z);
        float texel_to_units = max_side_length / extent.width;
        glm::vec3 light_pos = glm::vec3(
            std::floor((min.x + max.x) / (2 * texel_to_units)) * texel_to_units,
            std::floor((min.y + max.y) / (2 * texel_to_units)) * texel_to_units,
            min.z
        );
        vp.view = world_to_light;
        vp.view[3] = glm::vec4(-light_pos.x, -light_pos.y, -light_pos.z, 1.0f);
        uint32_t ds_index = cascade_index * frame_in_flight_count + frame_index;
        UniformAllocation uniform_alloc = per_frame_uniforms.allocate(sizeof(vp));
        memcpy(uniform_alloc.ptr, &vp, sizeof(vp));
        resource_manager->update_descriptor_set(
            directional_shadow_map_descriptor_sets[ds_index],
            {
                {
                    .binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .buffer_infos = {{ uniform_alloc.buffer, uniform_alloc.offset, sizeof(ViewProjection), }},
                },
                {
                    .binding = 1, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .buffer_infos = {{ dir_light_alloc.buffer, dir_light_alloc.offset, sizeof(DirectionalLight), }},
                },
            }
        );
        csm_uniform.ranges[cascade_index] = glm::vec4(range.x, range.y, 0.0f, 0.0f);
        if (cascade_index == 0) {
            csm_uniform.offsets[0] = glm::vec4(0.0f);
            csm_uniform.scales[0] = glm::vec4(1.0f);
            first_cascade_position = light_pos;
            first_cascade_side_length = max_side_length;
            first_cascade_z_range = glm::vec2(min.z, max.z);
            csm_uniform.first_cascade_view_proj = vp.proj * vp.view;
        } else {
            csm_uniform.offsets[cascade_index] = glm::vec4(
                2.0f * (first_cascade_position.x - light_pos.x) / max_side_length,
                2.0f * (first_cascade_position.y - light_pos.y) / max_side_length,
                (first_cascade_position.z - light_pos.z) / (max.z - min.z),
                1.0f
            );
            csm_uniform.scales[cascade_index] = glm::vec4(
                first_cascade_side_length / max_side_length,
                first_cascade_side_length / max_side_length,
                (first_cascade_z_range.y - first_cascade_z_range.x) / (max.z - min.z),
                1.0f
            );
        }
    }
    UniformAllocation csm_uniform_alloc = per_frame_uniforms.allocate(sizeof(csm_uniform));
    printf("Buffer: %d\n", csm_uniform_alloc.buffer.index);
    memcpy(csm_uniform_alloc.ptr, &csm_uniform, sizeof(csm_uniform));
    resource_manager->update_descriptor_set(
        csm_descriptor_sets[frame_index],
        {
            {
                .binding = 0, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .buffer_infos = {{ csm_uniform_alloc.buffer, csm_uniform_alloc.offset, sizeof(CsmUniform), }},
            },
            {
                .binding = 1, .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .buffer_infos = {{ dir_light_alloc.buffer, dir_light_alloc.offset, sizeof(DirectionalLight), }},
            },
        }
    );
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

void Application::generate_mipmaps(Morpho::Vulkan::CommandBuffer* cmd) {
    texture_barriers.resize(model.textures.size());
    uint32_t max_mip_level = 0;
    for (uint32_t i = 0; i < model.textures.size(); i++) {
        auto& texture = model.textures[i];
        auto gltf_image = model.images[texture.source];
        assert(gltf_image.component == 4);
        texture_barriers[i] = {
            .texture = textures[i],
            .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .mip_level_count = 1,
            .src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .src_access = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .dst_access = VK_ACCESS_TRANSFER_READ_BIT,
        };
        uint32_t mip_count = (uint32_t)std::bit_width((uint32_t)std::max(gltf_image.width, gltf_image.height));
        max_mip_level = std::max(max_mip_level, mip_count);
    }
    cmd->barrier(Morpho::make_const_span(texture_barriers.data(), texture_barriers.size()), {});
    texture_barriers.clear();
    for (uint32_t mip_level = 1; mip_level < max_mip_level; mip_level++) {
        for (uint32_t i = 0; i < model.textures.size(); i++) {
            auto& texture = model.textures[i];
            auto gltf_image = model.images[texture.source];
            uint32_t mip_count = (uint32_t)std::bit_width((uint32_t)std::max(gltf_image.width, gltf_image.height));
            if (mip_count <= mip_level) {
                continue;
            }
            cmd->blit({
                .src_texture = textures[i],
                .src_texture_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .dst_texture = textures[i],
                .dst_texture_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .regions = {{
                    .src_subresource = {
                        .mip_level = mip_level - 1,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                    .src_offsets = {
                        {0, 0, 0},
                        {
                            std::max(gltf_image.width >> (mip_level - 1), 1),
                            std::max(gltf_image.width >> (mip_level - 1), 1),
                            1
                        }
                    },
                    .dst_subresource = {
                        .mip_level = mip_level,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                    .dst_offsets = {
                        {0, 0, 0},
                        {
                            std::max(gltf_image.width >> (mip_level), 1),
                            std::max(gltf_image.width >> (mip_level), 1),
                            1
                        }
                    },

                }}
            });
            // TODO: keeps mips in Texture to avoid calculating mips and unnecessary barriers.
            texture_barriers.push_back({
                .texture = textures[i],
                .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .base_mip_level = mip_level,
                .mip_level_count = 1,
                .src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .src_access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .dst_access = VK_ACCESS_TRANSFER_READ_BIT,
            });
        }
        cmd->barrier(Morpho::make_const_span(texture_barriers.data(), texture_barriers.size()), {});
        texture_barriers.clear();
    }
    for (uint32_t i = 0; i < textures.size(); i++) {
        texture_barriers.push_back({
            .texture = textures[i],
            .old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .src_access = VK_ACCESS_TRANSFER_READ_BIT,
            .dst_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dst_access = VK_ACCESS_SHADER_READ_BIT
        });
    }
    cmd->barrier(Morpho::make_const_span(texture_barriers.data(), texture_barriers.size()), {});
    texture_barriers.clear();
}

void Application::draw_model(
    const tinygltf::Model& model,
    Morpho::DrawStream* draw_stream,
    Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
    Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
) {
    for (const auto& scene : model.scenes) {
        draw_scene(model, scene, draw_stream, normal_pipeline, double_sided_pipeline);
    }
    currently_bound_pipeline = {};
    current_material_index = -1;
}

void Application::draw_scene(
    const tinygltf::Model& model,
    const tinygltf::Scene& scene,
    Morpho::DrawStream* draw_stream,
    Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
    Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
) {
    for (const auto node_index : scene.nodes) {
        draw_node(model, model.nodes[node_index], draw_stream, normal_pipeline, double_sided_pipeline);
    }
}

void Application::draw_node(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    Morpho::DrawStream* draw_stream,
    Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
    Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
) {
    if (node.mesh >= 0) {
        draw_mesh(model, node.mesh, draw_stream, normal_pipeline, double_sided_pipeline);
    }
    for (uint32_t i = 0; i < node.children.size(); i++) {
        draw_node(model, model.nodes[node.children[i]], draw_stream, normal_pipeline, double_sided_pipeline);
    }
}

void Application::draw_mesh(
    const tinygltf::Model& model,
    uint32_t mesh_index,
    Morpho::DrawStream* draw_stream,
    Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
    Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
) {
    for (uint32_t i = 0; i < model.meshes[mesh_index].primitives.size(); i++) {
        draw_primitive(model, mesh_index, i, draw_stream, normal_pipeline, double_sided_pipeline);
    }
}

void Application::draw_primitive(
    const tinygltf::Model& model,
    uint32_t mesh_index,
    uint32_t primitive_index,
    Morpho::DrawStream* draw_stream,
    Morpho::Handle<Morpho::Vulkan::Pipeline> normal_pipeline,
    Morpho::Handle<Morpho::Vulkan::Pipeline> double_sided_pipeline
) {
    auto& primitive = model.meshes[mesh_index].primitives[primitive_index];
    if (primitive.attributes.size() != 4) {
        return;
    }
    if (primitive.material < 0) {
        std::cout << "Primitive with no material" << std::endl;
        return;
    }
    if (primitive.material != current_material_index) {
        auto& material = model.materials[primitive.material];
        draw_stream->bind_descriptor_set(material_descriptor_sets[primitive.material], 2);
        auto& pipeline_to_bind = material.doubleSided ? double_sided_pipeline : normal_pipeline;
        if (current_material_index < 0 || currently_bound_pipeline != pipeline_to_bind) {
            draw_stream->bind_pipeline(pipeline_to_bind);
            if (primitive_index != 0) {
                draw_stream->bind_descriptor_set(mesh_descriptor_sets[mesh_index], 3);
            }
            currently_bound_pipeline = pipeline_to_bind;
        }
        current_material_index = primitive.material;
    }
    if (primitive_index == 0) {
        draw_stream->bind_descriptor_set(mesh_descriptor_sets[mesh_index], 3);
    }
    for (auto& key_value : primitive.attributes) {
        if (attribute_name_to_location.find(key_value.first) == attribute_name_to_location.end()) {
            continue;
        }
        auto binding = attribute_name_to_binding.find(key_value.first)->second;
        auto accessor_index = key_value.second;
        auto& accessor = model.accessors[accessor_index];
        auto& buffer_view = model.bufferViews[accessor.bufferView];
        draw_stream->bind_vertex_buffer(
            buffers[buffer_view.buffer],
            binding,
            accessor.byteOffset + buffer_view.byteOffset
        );
    }
    if (primitive.indices >= 0) {
        auto& accessor = model.accessors[primitive.indices];
        auto& buffer_view = model.bufferViews[accessor.bufferView];
        auto index_type = gltf_to_index_type(accessor.type, accessor.componentType);
        assert(index_type == VK_INDEX_TYPE_UINT16);
        auto index_count = (uint32_t)accessor.count;
        draw_stream->bind_index_buffer(
            buffers[buffer_view.buffer],
            accessor.byteOffset + buffer_view.byteOffset
        );
        draw_stream->draw_indexed(
            index_count,
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
    return filter < 0 ? VK_FILTER_LINEAR : map.at(filter);
}

VkSamplerAddressMode Application::gltf_to_sampler_address_mode(int address_mode) {
    static const std::unordered_map<int, VkSamplerAddressMode> map = {
        {TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
        {TINYGLTF_TEXTURE_WRAP_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT},
        {TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT},
    };
    return map.at(address_mode);
}

Morpho::Handle<Morpho::Vulkan::Shader> Application::load_shader(const std::string& path) {
    auto code = read_file(path);
    auto stage = path.find(".vert") != std::string::npos
        ? Morpho::Vulkan::ShaderStage::VERTEX
        : Morpho::Vulkan::ShaderStage::FRAGMENT;
    return resource_manager->create_shader(code.data(), (uint32_t)code.size(), stage);
}

Morpho::DrawStream Application::acquire_draw_stream() {
    return draw_stream_pool.get_or_add([] { return Morpho::DrawStream{};});
}
