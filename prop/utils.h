#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>

namespace mtl {

static constexpr auto cacheln = 64;
static constexpr auto consume = std::memory_order_consume;
static constexpr auto relaxed = std::memory_order_relaxed;
static constexpr auto release = std::memory_order_release;

#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x) __builtin_expect(!!(x), 1)

template <typename T> void prefetch(T) {}
template <typename T> void prefetch(T *x) { __buildin_prefetch(x); }
template <typename T> void prefetch(const T *x) { __buildin_prefetch(x); }
template <typename T> void prefetch(std::vector<T> &x) {
    prefetch(x.data());
}
template <typename T> void prefetch(std::string &x) {
    prefetch(x.data());
}

}

#endif // UTILS_H
