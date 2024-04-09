#pragma once
#include <vulkan/vulkan.h>
#include "vma.hpp"
#include <optional>
#include <string>
#include "limits.hpp"
#include <stdexcept>
#include <vulkan/vulkan_core.h>
#include "common/span.hpp"

namespace Morpho::Vulkan {

enum class BufferMap {
    NONE,
    CAN_BE_MAPPED,
    PERSISTENTLY_MAPPED,
};

struct BufferInfo {
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO;
    BufferMap map = BufferMap::NONE;
    void* initial_data = nullptr;
    VkDeviceSize initial_data_size = 0;
};

class BufferInfoBuilder {
public:

private:
    BufferInfo info;
};

struct Buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    char* mapped;
};

struct TextureInfo {
    VkExtent3D extent;
    VkFormat format;
    VkImageUsageFlags image_usage;
    VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO;
    uint32_t array_layer_count = 1;
    uint32_t mip_level_count = 1;
    VkImageCreateFlags flags = 0;
};

struct Texture {
    VkFormat format;
    VkImageAspectFlags aspect;
    bool owns_image;
    VkImage image;
    VkImageView image_view;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

struct TextureSubresource {
    uint32_t              mip_level = 0;
    uint32_t              base_array_layer = 0;
    uint32_t              layer_count = VK_REMAINING_ARRAY_LAYERS;
};

struct SamplerInfo {
    VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_all = VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    VkFilter min_filter = VK_FILTER_LINEAR;
    VkFilter mag_filter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mip_filter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkBool32 compare_enable = VK_FALSE;
    VkCompareOp compare_op = VK_COMPARE_OP_NEVER;
    float min_lod = 0.0f;
    float max_lod = VK_LOD_CLAMP_NONE;
    float load_bias = 0.0f;
    float max_anisotropy = 0.0f;
};

struct Sampler {
    VkSampler sampler;
};

struct DescriptorSet {
    VkDescriptorSet descriptor_set;
    uint32_t set_index;
    VkPipelineLayout pipeline_layout;
};

struct TextureDescriptorInfo {
    Texture texture;
    Sampler sampler;
};

struct BufferDescriptorInfo {
    Buffer buffer;
    uint64_t offset;
    uint64_t range;
};

struct DescriptorSetUpdateRequest {
    uint32_t binding;
    VkDescriptorType descriptor_type;
    union {
        Span<const TextureDescriptorInfo> texture_infos;
        Span<const BufferDescriptorInfo> buffer_infos;
    };
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

struct FramebufferInfo {
    static constexpr uint32_t max_attachment_count = 8 + 1;
    RenderPassLayout layout;
    uint32_t attachment_count = 0;
    Texture attachments[max_attachment_count];
    VkExtent2D extent;
};

class FramebufferInfoBuilder {
public:
    FramebufferInfoBuilder& layout(const RenderPassLayout& layout);
    FramebufferInfoBuilder& attachment(Texture texture);
    FramebufferInfoBuilder& extent(VkExtent2D extent);
    FramebufferInfo info();
private:
    FramebufferInfo framebuffer_info;
};

struct Framebuffer {
    VkFramebuffer framebuffer;
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
    uint32_t max_descriptor_set_counts[Limits::MAX_DESCRIPTOR_SET_COUNT];
};

struct PipelineLayout {
    VkPipelineLayout pipeline_layout;
    // TODO: I think in the end I'll have to just create DescriptorSetLayout.
    VkDescriptorSetLayout descriptor_set_layouts[Limits::MAX_DESCRIPTOR_SET_COUNT];
    VkDescriptorPool descriptor_pools[Limits::MAX_DESCRIPTOR_SET_COUNT];
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
    bool depth_clamp_enabled;
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
