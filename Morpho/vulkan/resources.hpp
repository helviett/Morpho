#pragma once
#include <vulkan/vulkan.h>
#include "vma.hpp"
#include <optional>
#include <string>
#include "../common/hash_utils.hpp"
#include "limits.hpp"

namespace Morpho::Vulkan {

class Context;

class Buffer {
public:
    Buffer();
    Buffer(Context* context, VkBuffer buffer, VmaAllocation allocation, VmaAllocationInfo allocation_info);

    void update(const void* data, VkDeviceSize size);
    VkBuffer get_buffer() const;
    VmaAllocation get_allocation() const;
private:
    Context* context;
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

class DescriptorSet {
public:
    DescriptorSet();
    DescriptorSet(VkDescriptorSet descriptor_set);

    VkDescriptorSet get_descriptor_set() const;
private:
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

class RenderPassLayout {
public:
    RenderPassLayout();
    RenderPassLayout(RenderPassLayoutInfo info);
    RenderPassLayout(RenderPassLayoutInfo info, VkRenderPass render_pass);

    VkRenderPass get_vulkan_handle() const;
    const RenderPassLayoutInfo& get_info() const;
private:
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


class RenderPass {
public:
    RenderPass();
    RenderPass(VkRenderPass render_pass, RenderPassLayout layout);

    VkRenderPass get_vulkan_handle() const;
    RenderPassLayout get_render_pass_layout() const;
private:
    VkRenderPass render_pass;
    RenderPassLayout layout;
};

class Image {
public:
    Image();
    Image(VkImage image, VmaAllocation allocation, VmaAllocationInfo allocation_info);
    Image(VkImage image);

    VkImage get_image() const;
    VmaAllocation get_allocation() const;
private:
    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;
};

class ImageView {
public:
    ImageView();
    ImageView(VkImageView image_view, Image image = Image());

    VkImageView get_image_view() const;
    Image image;
private:
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

class Framebuffer {
public:
    Framebuffer(VkFramebuffer frambuffer);

    VkFramebuffer get_vulkan_handle();
private:
    VkFramebuffer framebuffer;
};

class Sampler {
public:
    Sampler();
    Sampler(VkSampler sampler);

    VkSampler get_sampler() const;
private:
    VkSampler sampler;
};

enum class ShaderStage {
    NONE = 0,
    VERTEX = 1,
    FRAGMENT = 2,
    MAX_VALUE = FRAGMENT,
};

class Shader {
public:
    Shader();
    Shader(const VkShaderModule module);
    Shader(const VkShaderModule module, const ShaderStage stage);
    Shader(const VkShaderModule module, const ShaderStage stage, const std::string& entry_point);

    void set_stage(const ShaderStage stage);
    void set_entry_point(const std::string& entry_point);
    ShaderStage get_stage() const;
    const std::string& get_entry_point() const;
    VkShaderModule get_shader_module() const;

    static VkShaderStageFlagBits shader_stage_to_vulkan(ShaderStage stage);
private:
    VkShaderModule shader_module;
    ShaderStage stage;
    std::string entry_point;
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
        Morpho::hash_combine(h, info.layout.get_info());
        return h;
    }
};