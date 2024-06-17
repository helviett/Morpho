#include <stdint.h>
#include <stb_ds.h>

namespace Morpho {

template<typename T>
class FramePool {
public:
    FramePool(const FramePool&) = delete;
    FramePool &operator=(const FramePool&) = delete;
    FramePool(FramePool&&) = delete;
    FramePool &operator=(FramePool&&) = delete;
    // otherwise I have to deal with mess of implicitly deleted constructor..
    FramePool() = default;
    ~FramePool() = default;
    static void init(FramePool<T>* pool, uint32_t frame_count);
    void destroy();
    template<typename LambdaT>
    void destroy(LambdaT&& destroy_object);
    bool try_get(T* value);
    template<typename LambdaT>
    T get_or_add(LambdaT&& add);
    void next_frame();
private:
    struct UsedResource {
        uint32_t frame;
        T resource;
    };

    uint32_t frame;
    uint32_t frame_count;
    T* pool;
    UsedResource* used;
};

template<typename T>
void FramePool<T>::init(FramePool<T>* pool, uint32_t frame_count) {
    pool->frame = 0;
    pool->frame_count = frame_count;
    pool->pool = nullptr;
    pool->used = nullptr;
}

template<typename T>
void FramePool<T>::destroy() {
    arrfree(pool);
    arrfree(used);
}

template<typename T>
template<typename LambdaT>
void FramePool<T>::destroy(LambdaT&& destroy_object) {
    for (uint32_t i = 0; i < arrsize(pool); i++) {
        destroy_object(pool[i]);
    }
    destroy();
}

template<typename T>
bool FramePool<T>::try_get(T* value) {
    if (arrlen(pool) != 0) {
        UsedResource used_resource = { .frame = frame, .resource = arrlast(pool), };
        arrput(used, used_resource);
        *value = arrpop(pool);
         return true;
    }
    return false;
}

template<typename T>
template<typename LambdaT>
T FramePool<T>::get_or_add(LambdaT&& add) {
    T value;
    if (try_get(&value)) {
        return value;
    }
    value = add();
    UsedResource used_resource = { .frame = frame, .resource = value, };
    arrput(used, used_resource);
    return value;
}

template<typename T>
void FramePool<T>::next_frame() {
    frame = (frame + 1) % frame_count;
    for (int32_t i = 0; i < arrlen(used); i++) {
        if (used[i].frame == frame) {
            arrput(pool, used[i].resource);
            arrdelswap(used, i);
            i--;
        }
    }
}

}
