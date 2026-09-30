// dyn_array.hpp - growable POD array backed by raylib's allocator
// (MemAlloc / MemRealloc / MemFree), used for hot particle storage.
#pragma once
#include "raylib.h"

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

namespace rd {

template <typename T>
class DynArray {
    static_assert(std::is_trivially_copyable<T>::value, "DynArray only holds trivially copyable types");

public:
    DynArray() = default;
    explicit DynArray(size_t cap) { reserve(cap); }
    ~DynArray() { release(); }
    DynArray(const DynArray&) = delete;
    DynArray& operator=(const DynArray&) = delete;
    DynArray(DynArray&& o) noexcept { swap(o); }
    DynArray& operator=(DynArray&& o) noexcept {
        if (this != &o) { release(); swap(o); }
        return *this;
    }

    void reserve(size_t cap) {
        if (cap <= cap_) return;
        const unsigned int bytes = static_cast<unsigned int>(cap * sizeof(T));
        T* p = data_ ? static_cast<T*>(MemRealloc(data_, bytes)) : static_cast<T*>(MemAlloc(bytes));
        if (!p) return;  // keep old buffer on OOM
        data_ = p;
        cap_ = cap;
    }
    T& push_back(const T& v) {
        if (size_ == cap_) reserve(cap_ ? cap_ * 2 : 16);
        data_[size_] = v;
        return data_[size_++];
    }
    // O(1) unordered removal
    void removeSwap(size_t i) {
        if (i >= size_) return;
        data_[i] = data_[size_ - 1];
        --size_;
    }
    void clear() { size_ = 0; }
    size_t size() const { return size_; }
    size_t capacity() const { return cap_; }
    bool empty() const { return size_ == 0; }
    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    void release() {
        if (data_) MemFree(data_);
        data_ = nullptr;
        size_ = cap_ = 0;
    }
    void swap(DynArray& o) noexcept {
        std::swap(data_, o.data_);
        std::swap(size_, o.size_);
        std::swap(cap_, o.cap_);
    }
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t cap_ = 0;
};

}  // namespace rd
