#pragma once
#include <initializer_list>
#include <cstdint>
#include <cassert>

namespace Morpho {

template<typename T>
class Span {
public:
    Span(): p_elems(nullptr), m_size(0) { }
    explicit Span(T* elem) : p_elems(elem), m_size(1) { }
    Span(T* elems, uint32_t size): p_elems(elems), m_size(size) { }
    Span(std::initializer_list<T> list): p_elems(list.begin()), m_size(list.size()) {}

    const T& operator[](uint32_t index) const {
        assert(index < m_size);
        return p_elems[index];
    }

    T& operator[](uint32_t index) {
        assert(index < m_size);
        return p_elems[index];
    }

    uint32_t size() const {
        return m_size;
    }

    T* data() const {
        return p_elems;
    }

private:
    T* p_elems;
    uint32_t m_size;
};

template<typename T>
Span<const T> make_const_span(const T* pointer, uint32_t size) {
    return Span(pointer, size);
}

template<typename T>
Span<const T> make_const_span(const T* pointer) {
    return Span(pointer);
}

}
