#pragma once

#include "com.h"
#include "vec.h"

namespace mtl {

template<typename T> struct Own {
    using Owned = T;
    T* data;
};
void init(Own<auto>& d) {
    d.data = nullptr;
}
void del(Own<NoDel>& d) {
    free(d.data);
}
void del(Own<Del>& d) {
    del(d->data);
    free(d.data);
}
template<Init T> bool make(Own<T>& d) {
    if ((d.data = mem::ualloc<T>(1)) == nullptr) {
        return false;
    }
    init(*d.data);
    return true;
}
template<typename T> bool make(Own<T>& d, const T& ele) {
    if ((d.data = mem::ealloc<T>(ele, 1)) == nullptr) {
        return false;
    }
    return true;
}

auto getnnull(Own<auto>& d) {
    return *d.data;
}

}
