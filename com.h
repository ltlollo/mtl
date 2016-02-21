#pragma once

#include <stddef.h>

namespace mtl {

template<typename T> concept bool Reloc   = requires(T d) {
    { reloc(d)      } -> void
};
template<typename T> concept bool Del     = requires(T d) {
    { del(d)        } -> void
};
template<typename T> concept bool Init    = requires(T d) {
    { init(d)       } -> void
};
template<typename T> concept bool At      = requires(T cont, size_t i) {
    { cont[i]       } -> auto
};
template<typename T> concept bool Copy    = requires(T d) {
    { copy(d, d)    } -> bool
};

template<typename T> concept bool NoReloc = !Reloc<T>;
template<typename T> concept bool NoDel   = !Del<T>;
template<typename T> concept bool NoInit  = !Init<T>;
template<typename T> concept bool NoAt    = !At<T>;
template<typename T> concept bool NoCopy  = !Copy<T>;

template<typename T> concept bool Cont    =
requires(T c, size_t i, typename T::Ele e) {
    { make(c, i, e) } -> bool;
    { init(c)       } -> void;
    { c.size        } -> size_t;
    { c[i]          } -> typename T::Ele;
};

template<typename T> concept bool DnCont  = requires(T c, const size_t i) {
    { reserve(c, i) } -> bool;
    { c.reserved    } -> size_t;
    requires Cont<T>;
};

}
