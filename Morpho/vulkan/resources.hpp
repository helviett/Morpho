#pragma once
#include <vulkan/vulkan.h>
#include "vma.hpp"
#include <optional>
#include <string>
#include "../common/hash_utils.hpp"
#include "limits.hpp"
#include <stdexcept>

namespace Morpho::Vulkan {

struct Buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

struct DescriptorSet {
    VkDescriptorSet descriptor_set;
};

struct SubpassInfo {
    static constexpr uint32_t max_color_attachment_count = 8;
    uint32_t color_attachments[max_color_attachment_count];
    uint32_t color_attachment_count = 0;
    std::optional<uint32_t> depth_attachment = std::nullopt;
    // uint32_t input_attachments[16];
    // uint32_t input_attachment_count = 0;
};

struct RenderPassLayoutAttachmentInfo {
    VkFormat format;
};

struct RenderPassLayoutInfo {
    static constexpr uint32_t max_attachment_count = 16;
    RenderPassLayoutAttachmentInfo attachments[max_attachment_count];
    uint32_t attachent_count = 0;
    SubpassInfo subpass;
};

class RenderPassLayoutInfoBuilder {
public:
    RenderPassLayoutInfoBuilder& attachment(VkFormat format);
    RenderPassLayoutInfoBuilder& subpass(
        std::initializer_list<uint32_t> color_attachments,
        std::optional<uint32_t> depth_attachment
    );
    // Should it return referece?
    RenderPassLayoutInfo info();
private:
    RenderPassLayoutInfo layout_info{};
};

struct RenderPassLayout {
    VkRenderPass render_pass;
    RenderPassLayoutInfo info;
};

struct RenderPassAttachmentInfo {
    VkAttachmentLoadOp load_op;
    VkAttachmentStoreOp store_op;
    VkImageLayout final_layout;
};

struct RenderPassInfo {
    static constexpr uint32_t max_attachment_count = 16;
    RenderPassAttachmentInfo attachments[max_attachment_count];
    uint32_t attachent_count = 0;
    RenderPassLayout layout;
};

class RenderPassInfoBuilder {
public:
    RenderPassInfoBuilder& layout(const RenderPassLayout& layout);
    RenderPassInfoBuilder& attachment(
        VkAttachmentLoadOp load_op,
        VkAttachmentStoreOp store_op,
        VkImageLayout final_layout
    );
    RenderPassInfo info();
private:
    RenderPassInfo render_pass_info;
};


struct RenderPass {
    VkRenderPass render_pass;
    RenderPassLayout layout;
};

struct Image {
    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

struct ImageView {
    Image image;
    VkImageView image_view;
};

struct FramebufferInfo {
    static constexpr uint32_t max_attachment_count = 8 + 1;
    RenderPassLayout layout;
    uint32_t attachment_count = 0;
    ImageView attachments[max_attachment_count];
    VkExtent2D extent;
};

class FramebufferInfoBuilder {
public:
    FramebufferInfoBuilder& layout(const RenderPassLayout& layout);
    FramebufferInfoBuilder& attachment(ImageView image_view);
    FramebufferInfoBuilder& extent(VkExtent2D extent);
    FramebufferInfo info();
private:
    FramebufferInfo framebuffer_info;
};

struct Framebuffer {
    VkFramebuffer framebuffer;
};

struct Sampler {
    VkSampler sampler;
};

enum class ShaderStage {
    NONE = 0,
    VERTEX = 1,
    FRAGMENT = 2,
    MAX_VALUE = FRAGMENT,
};

inline VkShaderStageFlagBits shader_stage_to_vulkan(ShaderStage stage) {
    switch (stage)
    {
    case ShaderStage::NONE:
        throw std::runtime_error("Invalid shader stage.");
    case ShaderStage::VERTEX:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::FRAGMENT:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    default:
        throw std::runtime_error("Not implemented");
    }
}

struct Shader {
    VkShaderModule shader_module;
    ShaderStage stage;
};

struct PipelineLayoutInfo {
    VkDescriptorSetLayoutBinding* set_binding_infos[Limits::MAX_DESCRIPTOR_SET_COUNT];
    uint32_t set_binding_count[Limits::MAX_DESCRIPTOR_SET_COUNT];
};

struct PipelineLayout {
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_set_layouts[Limits::MAX_DESCRIPTOR_SET_COUNT];
};

struct PipelineInfo {
    Shader* shaders;
    uint32_t shader_count;
    VkPrimitiveTopology primitive_topology;
    VkVertexInputAttributeDescription* attributes;
    uint32_t attribute_count;
    VkVertexInputBindingDescription* bindings;
    uint32_t binding_count;
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;
    float depth_bias_constant_factor;
    float depth_bias_slope_factor;
    bool depth_test_enabled;
    bool depth_write_enabled;
    VkCompareOp depth_compare_op;
    VkPipelineColorBlendAttachmentState blend_state;
    RenderPassLayout* render_pass_layout;
    PipelineLayout* pipeline_layout;
};

struct Pipeline {
    VkPipeline pipeline;
    // TODO: Remove
    PipelineLayout pipeline_layout;
};

}

template<>
struct std::hash<Morpho::Vulkan::RenderPassLayoutAttachmentInfo> {
    std::size_t operator()(Morpho::Vulkan::RenderPassLayoutAttachmentInfo const& info) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, info.format);
        return h;
    }
};

template<>
struct std::hash<Morpho::Vulkan::RenderPassLayoutInfo> {
    std::size_t operator()(Morpho::Vulkan::RenderPassLayoutInfo const& info) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, info.max_attachment_count);
        for (std::size_t i = 0; i < info.attachent_count; i++) {
            Morpho::hash_combine(h, info.attachments[i]);
        }
        return h;
    }
};

template<>
struct std::hash<Morpho::Vulkan::RenderPassAttachmentInfo> {
    std::size_t operator()(Morpho::Vulkan::RenderPassAttachmentInfo const& info) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, info.load_op, info.store_op, info.final_layout);
        return h;
    }
};

template<>
struct std::hash<Morpho::Vulkan::RenderPassInfo> {
    std::size_t operator()(Morpho::Vulkan::RenderPassInfo const& info) const noexcept {
        std::size_t h = 0;
        Morpho::hash_combine(h, info.max_attachment_count);
        for (std::size_t i = 0; i < info.attachent_count; i++) {
            Morpho::hash_combine(h, info.attachments[i]);
        }
        Morpho::hash_combine(h, info.layout.info);
        return h;
    }
};