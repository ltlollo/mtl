#pragma once

#include "com.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

namespace mtl {

namespace mem {
namespace raw {
template <typename T> T *ualloc(const size_t n) {
    return (T *)malloc(n * sizeof(T));
}
template <typename T> T *zalloc(const size_t n) {
    return (T *)calloc(n, sizeof(T));
}
template <typename T> T *ralloc(T *raw, const size_t, const size_t n) {
    return (T *)realloc(raw, n * sizeof(T));
}
}

template <typename T> T *ualloc(const size_t n) { return raw::ualloc<T>(n); }
template <NoReloc T> T *zalloc(const size_t n) { return raw::zalloc<T>(n); }
template <NoReloc T> T *ralloc(T *raw, const size_t _, const size_t n) {
    return raw::ralloc(raw, _, n);
}
template <Reloc T> T *ralloc(T *raw, const size_t os, const size_t ns) {
    T *res;
    if ((res = raw::ralloc(raw, os, ns)) == nullptr) {
        return res;
    }
    for (size_t i = os; i < ns; ++i) {
        reloc(res[i]);
    }
    return res;
}
template <NoReloc T> void cpy(T *dst, T *src, const size_t size) {
    memcpy(dst, src, size * sizeof(T));
}
template <Reloc T> void cpy(T *dst, T *src, const size_t size) {
    memcpy(dst, src, size * sizeof(T));
    for (size_t i = 0; i < size; ++i) {
        reloc(dst[i]);
    }
}
template <typename T> bool iszero(const T &ele) {
    char zero[sizeof(T)] = {0};
    return bcmp(&zero, &ele, sizeof(T)) == 0;
}
template <typename T> T *ealloc(const T &ele, const size_t size) {
    if (mem::iszero(ele)) {
        return mem::zalloc<T>(size);
    }
    T *res;
    if ((res = mem::ualloc<T>(size)) == nullptr) {
        return res;
    }
    for (size_t i = 0; i < size; ++i) {
        res[i] = ele;
    }
    return res;
}
template <typename T> void ezero(T *raw, const size_t size) {
    memset(raw, 0, sizeof(T) * size);
}
}

template <typename T> struct Vec {
    using Owned = T;
    size_t size = 0;
    size_t reserved = 0;
    T *data = nullptr;
    T &operator[](const size_t i) {
        assert(i < size);
        return data[i];
    }
};
bool make(Vec<NoReloc> &vec, const size_t size, const NoReloc &ele) {
    if (vec.size) {
        return false;
    }
    if ((vec.data = mem::ealloc(ele, size)) == nullptr) {
        return false;
    }
    vec.size = size;
    vec.reserved = size;
    return true;
}
bool reserve(Vec<auto> &vec, const size_t ns) {
    if (ns < vec.reserved) {
        return true;
    }
    auto res = mem::ralloc(vec.data, vec.size, ns);
    if (res == nullptr) {
        return false;
    }
    vec.data = res;
    vec.reserved = ns;
    return true;
}
template <Init T> bool make(Vec<T> &vec, const size_t size) {
    if (vec.size) {
        return false;
    }
    if ((vec.data = mem::ualloc<T>(size)) == nullptr) {
        return false;
    }
    vec.reserved = size;
    for (size_t i = 0; i < (vec.size = size); ++i) {
        init(vec[i]);
    }
    return true;
}
template <DnCont T, DnCont U> bool merge(T &dst, U &src) {
    size_t ns = dst.size + src.size;
    if (reserve(dst, ns) == false) {
        return false;
    }
    mem::cpy(dst.data + dst.size, src.data, src.size);
    src.size = 0;
    dst.size = ns;
    return true;
}
DnCont { T }
bool push(T &dst, const auto &ele, const size_t initn = 1) {
    size_t ns;
    if (dst.size < dst.reserved) {
        dst[dst.size++] = ele;
        return true;
    }
    ns = dst.reserved ? 2 * dst.reserved : initn;
    if (reserve(dst, ns) == false) {
        return false;
    }
    dst[dst.size++] = ele;
    return true;
}
DnCont { T }
bool pusham(T &dst, const auto &ele, const size_t initn = 1) {
    size_t ns;
    if (dst.size < dst.reserved) {
        dst[dst.size++] = ele;
        return true;
    }
    ns = dst.reserved ? dst.reserved + dst.reserved / 2 : initn;
    if (reserve(dst, ns) == false) {
        return false;
    }
    dst[dst.size++] = ele;
    return true;
}

Cont { C }
void del(C &vec) requires Del<typename C::Owned> {
    for (size_t i = 0; i < vec.size; ++i) {
        del(vec[i]);
    }
    free(vec.data);
    init(vec);
}
Cont { C }
void del(C &vec) requires NoDel<typename C::Owned> {
    free(vec.data);
    init(vec);
}

void init(Vec<auto> &vec) {
    vec.size = 0;
    vec.reserved = 0;
    vec.data = nullptr;
}

template <typename T, size_t N = 16> struct MuVec {
    using Owned = T;
    size_t size = 0;
    size_t reserved = N;
    T *data = mem;
    T mem[N] = {0};
    T &operator[](const size_t i) {
        assert(i < size);
        return data[i];
    }
};
template <size_t N> void init(MuVec<auto, N> &vec) {
    vec.size = 0;
    vec.reserved = N;
    vec.data = vec.mem;
    mem::ezero(vec.mem, N);
}
template <size_t N>
bool make(MuVec<NoReloc, N> &vec, const size_t size, const NoReloc &ele) {
    if (vec.size) {
        return false;
    }
    if (vec.reserved > size) {
        for (size_t i = 0; i < (vec.size = size); ++i) {
            vec[i] = ele;
        }
        return true;
    }
    if ((vec.data = mem::ealloc(ele, size)) == nullptr) {
        return false;
    }
    vec.size = size;
    vec.reserved = size;
    return true;
}
template <Init T, size_t N> bool make(MuVec<T, N> &vec, const size_t size) {
    if (vec.size) {
        return false;
    }
    if (reserve(vec, size) == false) {
        return false;
    }
    for (size_t i = 0; i < (vec.size = size); ++i) {
        init(vec[i]);
    }
    return true;
}
template <typename T, size_t N>
requires Del<T> void cutoff(MuVec<T, N> &vec, const size_t size) {
    if (vec.size < size) {
        return;
    }
    for (size_t i = size; i < vec.size; ++i) {
        del(vec[i]);
    }
    vec.size = size;
}
template <typename T, size_t N>
requires NoDel<T> void cutoff(MuVec<T, N> &vec, const size_t size) {
    if (vec.size < size) {
        return;
    }
    vec.size = size;
}
DnCont { C }
void cutoff(C &vec, const size_t size) requires Del<typename C::Owned> {
    if (vec.size < size) {
        return;
    }
    for (size_t i = size; i < vec.size; ++i) {
        del(vec[i]);
    }
    vec.size = size;
}
DnCont { C }
void cutoff(C &vec, const size_t size) requires NoDel<typename C::Owned> {
    if (vec.size < size) {
        return;
    }
    vec.size = size;
}
template <Del T, size_t N> void shrink(MuVec<T, N> &vec, const size_t size) {
    if (vec.size < size) {
        return;
    }
    for (size_t i = size; i < vec.size; ++i) {
        del(vec[i]);
    }
    vec.size = size;
    if (vec.reserved == N) {
        return;
    } else if (vec.reserved > N && size > N) {
        mem::ralloc(vec.data, vec.reserved, vec.size);
        vec.reserved = size;
        return;
    } else {
        mem::cpy(vec.data, vec.mem, size);
        free(vec.data);
        vec.data = vec.mem;
        vec.reserved = N;
    }
}
template <NoDel T, size_t N> void shrink(MuVec<T, N> &vec, const size_t size) {
    if (vec.size < size) {
        return;
    }
    vec.size = size;
    if (vec.reserved == N) {
        return;
    } else if (vec.reserved > N && size > N) {
        mem::ralloc(vec.data, vec.reserved, vec.size);
        vec.reserved = size;
        return;
    } else {
        mem::cpy(vec.data, vec.mem, size);
        free(vec.data);
        vec.data = vec.mem;
        vec.reserved = N;
    }
}
template <size_t N> void reloc(MuVec<auto, N> &vec) {
    if (vec.reserved > N) {
        return;
    }
    vec.data = vec.mem;
}
template <size_t N> void del(MuVec<Del, N> &vec) {
    for (size_t i = 0; i < vec.size; ++i) {
        del(vec[i]);
    }
    if (vec.size > N) {
        free(vec.data);
    }
    init(vec);
}
template <size_t N> void del(MuVec<NoDel, N> &vec) {
    if (vec.size > N) {
        free(vec.data);
    }
    init(vec);
}
template <typename T, size_t N>
bool reserve(MuVec<T, N> &vec, const size_t ns) {
    T *res;
    if (ns < vec.reserved) {
        return true;
    }
    if (vec.size < N) {
        if ((res = mem::ualloc<T>(ns)) != nullptr) {
            mem::cpy(res, vec.data, vec.size);
        }
    } else {
        res = mem::ralloc(vec.data, vec.size, ns);
    }
    if (res == nullptr) {
        return false;
    }
    vec.data = res;
    vec.reserved = ns;
    return true;
}

template <typename T> struct FixVec {
    using Owned = T;
    size_t size = 0;
    T *data = nullptr;
    T &operator[](const size_t i) {
        assert(i < size);
        return data[i];
    }
};
bool make(FixVec<NoReloc> &vec, const size_t size, const NoReloc &ele) {
    if ((vec.data = mem::ealloc(ele, size)) == nullptr) {
        return false;
    }
    vec.size = size;
    return true;
}
template <Init T> bool make(FixVec<T> &vec, const size_t size) {
    if (vec.size) {
        return false;
    }
    if ((vec.data = mem::ualloc<T>(size)) == nullptr) {
        return false;
    }
    for (size_t i = 0; i < (vec.size = size); ++i) {
        init(vec[i]);
    }
    return true;
}
void init(FixVec<auto> &vec) {
    vec.size = 0;
    vec.data = nullptr;
}

template <DnCont T, DnCont U>
bool copy(T &dst, U &src) requires Copy<typename U::Owned> {
    cutoff(dst, 0);
    if (reserve(dst, src.size) == false) {
        return false;
    }
    mem::cpy(dst.data, src.data, src.size);
    for (size_t i = 0; i < (dst.size = src.size); ++i) {
        if (copy(src[i], dst[i]) == false) {
            dst.size = i;
            return false;
        }
    }
    return true;
}
template <DnCont T, DnCont U>
bool copy(T &dst, U &src) requires NoCopy<typename U::Owned> {
    cutoff(dst, 0);
    if (reserve(dst, src.size) == false) {
        return false;
    }
    mem::cpy(dst.data, src.data, src.size);
    dst.size = src.size;
    return true;
}

DnCont { T }
bool resize(T &vec, const size_t n, const NoReloc &ele) {
    if (vec.size == n) {
    } else if (vec.size > n) {
        cutoff(vec, n);
        vec.size = n;
    } else {
        if (reserve(vec, n) == false) {
            return false;
        }
        size_t i = vec.size;
        for (; i < (vec.size = n); ++i) {
            vec[i] = ele;
        }
    }
    return true;
}
DnCont { T }
bool resize(T &vec, const size_t n) requires Init<typename T::Owned> {
    if (vec.size == n) {
    } else if (vec.size > n) {
        cutoff(vec, n);
        vec.size = n;
    } else {
        if (reserve(vec, n) == false) {
            return false;
        }
        size_t i = vec.size;
        for (; i < (vec.size = n); ++i) {
            init(vec[i]);
        }
    }
    return true;
}
}
