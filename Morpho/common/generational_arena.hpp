#pragma once
#include <stdint.h>
#include <stb_ds.h>

namespace Morpho {

template<typename T>
struct Handle
{
    static Handle<T> null();

    uint16_t index;
    uint16_t gen;
};

template<typename T>
Handle<T> Handle<T>::null() {
    return { .index = UINT16_MAX, .gen = UINT16_MAX };
}

template<typename T>
bool operator==(const Handle<T>& lhs, const Handle<T>& rhs)
{
    return lhs.index == rhs.index && lhs.gen == rhs.gen;
}

template<typename T>
bool operator!=(const Handle<T>& lhs, const Handle<T>& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
class GenerationalArena {
public:
    GenerationalArena();

    Handle<T> add(T data);
    T get(Handle<T> handle);
    bool try_get(Handle<T> handle, T* value);
    bool is_valid(Handle<T> handle);
    void remove(Handle<T> handle);
private:
    T* data = nullptr;
    uint16_t* free_list = nullptr;
    uint16_t* gens = nullptr;
};

template<typename T>
Handle<T> GenerationalArena<T>::add(T value) {
    if (arrlen(free_list) != 0) {
        uint16_t index = arrpop(free_list);
        return { .index = index, .gen = gens[index], };
    }
    arrput(data, value);
    arrput(gens, 0u);
    return { .index = (uint16_t)(arrlenu(data) - 1U), .gen = (uint16_t)0 };
}

template<typename T>
T GenerationalArena<T>::get(Handle<T> handle) {
    return data[handle.index];
}

template<typename T>
bool GenerationalArena<T>::try_get(Handle<T> handle, T* value) {
    if (handle.index >= arrlen(data) || gens[handle.index] != handle.gen) {
        return false;
    }
    *value = data[handle.index];
    return true;
}

template<typename T>
bool GenerationalArena<T>::is_valid(Handle<T> handle) {
    return handle.index < arrlen(data) && gens[handle.index] == handle.gen;
}

template<typename T>
void GenerationalArena<T>::remove(Handle<T> handle) {
    assert(is_correct(handle));
    gens[handle.index]++;
    arrput(free_list, handle.gen);
}


}
