#pragma once
#include <vulkan/vulkan.h>
#include "shader.hpp"
#include "pipeline.hpp"
#include "buffer.hpp"
#include "pipeline_layout.hpp"
#include "limits.hpp"
#include "../common/hash_utils.hpp"

namespace Morpho::Vulkan {

class PipelineState {
public:
    PipelineState();

    void add_shader(const Shader shader);
    uint32_t get_shader_count() const;
    Shader get_shader(int index) const;
    void clear_shaders();
    void add_vertex_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);
    uint32_t get_attribute_description_count() const;
    void add_vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
    uint32_t get_vertex_binding_description_count() const;
    VkVertexInputAttributeDescription get_vertex_attribute_description(uint32_t index) const;
    VkVertexInputBindingDescription get_vertex_binding_description(uint32_t index) const;
    uint32_t get_occupied_descriptor_set_count() const;
    void clear_vertex_attribute_descriptions();
    void clear_vertex_binding_descriptions();
    void set_pipeline_layout(PipelineLayout pipeline_layout);
    PipelineLayout get_pipeline_layout() const;
    bool get_is_dirty() const;
    bool get_and_clear_is_dirty();
    void set_depth_state(VkBool32 test_enable, VkBool32 write_enable, VkCompareOp compare_op);
    VkPipelineDepthStencilStateCreateInfo get_depth_stencil_state() const;
    void set_front_face(VkFrontFace front_face);
    VkFrontFace get_front_face() const;
    void set_cull_mode(VkCullModeFlags cull_mode);
    VkCullModeFlags get_cull_mode() const;
    void set_topology(VkPrimitiveTopology topology);
    VkPrimitiveTopology get_topology() const;
    void enable_depth_bias(float depth_bias_constant_factor, float depth_bias_slope_factor);
    void disable_depth_bias();
    void enable_blending(
        VkBlendFactor src_color_blend_factor,
        VkBlendFactor dst_color_blend_factor,
        VkBlendOp color_blend_op,
        VkBlendFactor src_alpha_blend_factor,
        VkBlendFactor dst_alpha_blend_factor,
        VkBlendOp alpha_blend_op
    );
    void disable_blending();
    VkPipelineColorBlendAttachmentState get_blending_state() const;
    VkBool32 get_depth_bias_enable() const;
    float get_depth_bias_constant_factor() const;
    float get_depth_bias_slope_factor() const;
private:
    bool is_dirty = true;
    Shader shaders[2];
    uint32_t shader_count = 0;
    VkVertexInputAttributeDescription attribute_descriptions[Limits::MAX_VERTEX_ATTRIBUTE_DESCRIPTION_COUNT];
    uint32_t attribute_description_count = 0;
    VkVertexInputBindingDescription binding_descriptions[Limits::MAX_VERTEX_INPUT_BINDING_COUNT];
    uint32_t binding_description_count = 0;
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
    PipelineLayout pipeline_layout;
    VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32 depth_bias_enable = VK_FALSE;
    float depth_bias_constant_factor = 0.0;
    float depth_bias_slope_factor = 0.0;
    VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
};

}

template<>
struct std::hash<Morpho::Vulkan::PipelineState> {
    std::size_t operator()(Morpho::Vulkan::PipelineState const& state) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, state.get_shader_count());
        for (uint32_t i = 0; i < state.get_shader_count(); i++) {
            auto shader = state.get_shader(i);
            Morpho::hash_combine(h, shader.get_stage(), shader.get_shader_module(), shader.get_entry_point());
        }
        Morpho::hash_combine(h, state.get_attribute_description_count());
        for (uint32_t i = 0; i < state.get_attribute_description_count(); i++) {
            auto desc = state.get_vertex_attribute_description(i);
            Morpho::hash_combine(h, desc.location, desc.binding, desc.format, desc.offset);
        }
        Morpho::hash_combine(h, state.get_vertex_binding_description_count());
        for (uint32_t i = 0; i < state.get_vertex_binding_description_count(); i++) {
            auto desc = state.get_vertex_binding_description(i);
            Morpho::hash_combine(h, desc.binding, desc.inputRate, desc.stride);
        }
        Morpho::hash_combine(h, state.get_pipeline_layout().get_pipeline_layout());
        auto depth_stencil = state.get_depth_stencil_state();
        Morpho::hash_combine(
            h,
            depth_stencil.depthTestEnable,
            depth_stencil.depthWriteEnable,
            depth_stencil.depthCompareOp
        );
        Morpho::hash_combine(h, state.get_front_face(), state.get_cull_mode(), state.get_topology());
        auto blending = state.get_blending_state();
        if (blending.blendEnable) {
            Morpho::hash_combine(
                h,
                blending.alphaBlendOp,
                blending.colorBlendOp,
                blending.colorWriteMask,
                blending.dstAlphaBlendFactor,
                blending.dstColorBlendFactor,
                blending.srcAlphaBlendFactor,
                blending.srcColorBlendFactor
            );
        }
        Morpho::hash_combine(
            h,
            state.get_depth_bias_enable(),
            state.get_depth_bias_constant_factor(),
            state.get_depth_bias_slope_factor()
        );
        return h;
    }
};
