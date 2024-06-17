#include "vulkan/resources.hpp"

namespace Morpho {

// NOTE: Unoptimized stream implementaiton.
class DrawStream {
public:
    DrawStream() = default;

    void draw_indexed(uint32_t index_count, uint32_t index_offset);
    void bind_descriptor_set(Handle<Vulkan::DescriptorSet> ds, uint32_t set_index);
    void bind_vertex_buffer(Handle<Vulkan::Buffer> buffer, uint32_t binding, uint32_t offset);
    void bind_index_buffer(Handle<Vulkan::Buffer> buffer, uint32_t offset);
    void bind_pipeline(Handle<Vulkan::Pipeline> pipeline);
    void clear_state();
    uint8_t* get_stream();
    uint64_t get_size();
    void destroy();

    // just keep it public for simplicity for now
    struct DrawCall {
        Handle<Vulkan::DescriptorSet> descriptor_sets[3];
        Handle<Vulkan::Pipeline> pipeline;
        Handle<Vulkan::Buffer> index_buffer;
        uint32_t index_buffer_offset;
        Handle<Vulkan::Buffer> vertex_buffers[4];
        uint32_t vertex_buffer_offsets[4];
        uint16_t index_offset;
        uint16_t index_count;

        static DrawCall null();
    };
private:
    uint8_t* stream = nullptr;
    DrawCall current_draw_call = DrawCall::null();
};

}
