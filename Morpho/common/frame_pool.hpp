#include <stdint.h>
#include <stb_ds.h>

namespace Morpho {

template<typename T>
class FramePool {
public:
    typedef T (*add_fn)(void* user_data);
    typedef void (*reset_fn)(T* item, void* user_data);

    struct FramePoolInfo {
        uint64_t frames_in_flight_count;
        void* user_data;
        add_fn add;
        reset_fn reset;
    };

    FramePool(const FramePool&) = delete;
    FramePool &operator=(const FramePool&) = delete;
    FramePool(FramePool&&) = delete;
    FramePool &operator=(FramePool&&) = delete;
    // otherwise I have to deal with mess of implicitly deleted constructor..
    FramePool() = default;
    ~FramePool() = default;
    static void init(FramePool<T>* pool, const FramePool<T>::FramePoolInfo& info);
    void destroy();
    template<typename LambdaT>
    void destroy(LambdaT&& destroy_object);
    bool try_get(T* value);
    template<typename LambdaT>
    T get_or_add(LambdaT&& add);
    T get_or_add();
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
    add_fn add;
    reset_fn reset;
    void* user_data;
};

template<typename T>
void FramePool<T>::init(FramePool<T>* pool, const FramePool<T>::FramePoolInfo& info) {
    assert(info.frames_in_flight_count != 0);
    pool->frame = 0;
    pool->frame_count = info.frames_in_flight_count;
    pool->pool = nullptr;
    pool->used = nullptr;
    pool->add = info.add;
    pool->reset = info.reset;
    pool->user_data = info.user_data;
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
T FramePool<T>::get_or_add() {
    T value;
    if (try_get(&value)) {
        return value;
    }
    value = add(user_data);
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
            if (reset != nullptr) {
                reset(&arrlast(pool), user_data);
            }
            arrdelswap(used, i);
            i--;
        }
    }
}

}
