#include "common/draw_stream.hpp"

namespace Morpho {

DrawStream::DrawCall DrawStream::DrawCall::null() {
    Handle<Vulkan::DescriptorSet> null_ds = Handle<Vulkan::DescriptorSet>::null();
    Handle<Vulkan::Buffer> null_buffer = Handle<Vulkan::Buffer>::null();
    return {
        .descriptor_sets = { null_ds, null_ds, null_ds },
        .pipeline = Handle<Vulkan::Pipeline>::null(),
        .index_buffer = null_buffer,
        .vertex_buffers = { null_buffer, null_buffer, null_buffer, null_buffer, },
        .index_offset = 0,
        .index_count = 0,
    };
}

void DrawStream::draw_indexed(uint32_t index_count, uint32_t index_offset) {
    current_draw_call.index_count = index_count;
    current_draw_call.index_offset = index_offset;
    uint64_t stream_size = arrlen(stream);
    arrsetlen(stream, stream_size + sizeof(current_draw_call));
    memcpy(&stream[stream_size], &current_draw_call, sizeof(current_draw_call));
}

void DrawStream::bind_descriptor_set(Handle<Vulkan::DescriptorSet> ds, uint32_t set_index) {
    assert(set_index != 0);
    current_draw_call.descriptor_sets[set_index - 1] = ds;
}

void DrawStream::bind_vertex_buffer(Handle<Vulkan::Buffer> buffer, uint32_t binding, uint32_t offset) {
    current_draw_call.vertex_buffers[binding] = buffer;
    current_draw_call.vertex_buffer_offsets[binding] = offset;
}

void DrawStream::bind_index_buffer(Handle<Vulkan::Buffer> buffer, uint32_t offset) {
    current_draw_call.index_buffer = buffer;
    current_draw_call.index_buffer_offset = offset;
}

void DrawStream::bind_pipeline(Handle<Vulkan::Pipeline> pipeline) {
    current_draw_call.pipeline = pipeline;
}

void DrawStream::clear_state() {
    current_draw_call = DrawCall::null();
}

uint8_t* DrawStream::get_stream() {
    return stream;
}

uint64_t DrawStream::get_size() {
    return arrlen(stream);
}

void DrawStream::destroy() {
    arrfree(stream);
}

void DrawStream::reset() {
    arrsetlen(stream, 0);
}

}
