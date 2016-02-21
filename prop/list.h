#pragma once

#include <type_traits>
#include <atomic>
#include <utility>
#include <cstddef>

static constexpr auto cacheln = 64;
static constexpr auto consume = std::memory_order_consume;
static constexpr auto relaxed = std::memory_order_relaxed;
static constexpr auto release = std::memory_order_release;

namespace mtl {

template<typename T> struct alignas(cacheln) Ele {
    std::atomic<Ele<T>*> next;
    T data;
    Ele() noexcept {
        next = nullptr;
    }
    Ele(T&& value) noexcept : data{std::move(value)} {
        static_assert(std::is_nothrow_move_constructible<T>());
        static_assert(std::is_default_constructible<T>());
    }
    Ele(const T& value) noexcept : data{value} {
        static_assert(std::is_nothrow_copy_constructible<T>());
    }
};
template<typename T> struct alignas(cacheln) Ele<T*> {
    std::atomic<Ele<T>*> next;
    T *data;
    Ele() noexcept {
        data = nullptr;
        next = nullptr;
    }
    Ele(T* value) noexcept : data{value} {
    }
    ~Ele() noexcept {
        delete data;
    }
};

template<typename T> struct MtList {
    Ele<T> trampoline = {};
};

template<typename T> void chain(MtList<T>& q, Ele<T>* ele) noexcept {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    do {
        while ((curr = curr->next.exchange(curr, consume)) == prev) {
            continue;
        }
        if (curr == nullptr) {
            prev->next.store(ele, relaxed);
            return;
        }
        prev->next.store(curr, relaxed);
        prev = curr;
    } while (true);
}
template<typename T>
void apply(MtList<T>& q, auto filt, auto pred, bool cont = true) noexcept {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    Ele<T> *next;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (curr) {
        next = curr;
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        if (filt(curr->data)) {
            pred(curr);
            if (!cont) {
                prev->next.store(next, relaxed);
                return;
            }
            curr = next;
        } else {
            prev->next.store(curr, relaxed);
            prev = curr;
            curr = next;
        }
    }
    prev->next.store(nullptr, relaxed);
}
template<typename T>
void applyzip(MtList<T>& q, auto filt, auto pred, bool cont = true) noexcept {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    Ele<T> *next;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (curr) {
        next = curr;
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        if (filt(curr->data, next)) {
            pred(curr);
            if (!cont) {
                prev->next.store(next, relaxed);
                return;
            }
            curr = next;
        } else {
            prev->next.store(curr, relaxed);
            prev = curr;
            curr = next;
        }
    }
    prev->next.store(nullptr, relaxed);
}
template<typename T>
bool insert(MtList<T>& q, auto* head, auto* tail, auto pred) noexcept {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    do {
        while ((curr = curr->next.exchange(curr, consume)) == prev) {
            continue;
        }
        if (pred(prev, curr)) {
            tail->next.store(curr, relaxed);
            prev->next.store(head, release);
            return true;
        }
        prev->next.store(curr, relaxed);
        if (curr == nullptr) {
            return false;
        }
        prev = curr;
    } while (true);
}

template<typename T>
bool insert(MtList<T>& q, Ele<T>* ele, auto pred) noexcept {
    return insert(q, ele, ele, pred);
}

template<typename T>
void push(MtList<T>& q, Ele<T>* head, Ele<T>* tail) noexcept {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    tail->next.store(curr, relaxed);
    prev->next.store(head, release);
}

template<typename T> void push(MtList<T>& q, Ele<T>* ele) noexcept {
    push(q, ele, ele);
}
template<typename T> auto get(MtList<T*>& q, auto filt) noexcept {
    T *res = nullptr;
    apply(q, filt, [&](auto* ele) {
          std::swap(res, ele->data);
          delete ele;
    }, false);
    return res;
}
template<typename T> auto get(MtList<T>& q, auto filt) noexcept {
    T res = {};
    apply(q, filt, [&](auto* ele) {
          res = std::move(ele->data);
          delete ele;
    }, false);
    return res;
}
size_t rm(MtList<auto>& q, auto filt) noexcept {
    size_t n = 0;
    apply(q, filt, [&](auto* ele) {
          delete ele;
          ++n;
    });
    return n;
}
template<typename T> auto last(MtList<T>& q) noexcept {
    T res = {};
    applyzip(q, [](T, Ele<T>* nx) {
          return nx == nullptr;
    }, [&](auto* ele) {
          res = std::move(ele->data);
          delete ele;
    }, false);
    return res;
}
template<typename T> auto last(MtList<T*>& q) noexcept {
    T *res = nullptr;
    applyzip(q, [](T, Ele<T>* nx) {
          return nx == nullptr;
    }, [&](auto* ele) {
          std::swap(res, ele->data);
          delete ele;
    }, false);
    return res;
}
template<typename T> bool rmlast(MtList<T>& q) noexcept {
    Ele<T> *res = nullptr;
    applyzip(q, [](auto, auto* nx) {
        return nx == nullptr;
    }, [&](auto* ele) {
        res = ele;
    }, false);
    if (res) {
        delete res;
        return true;
    }
    return false;
}
template<typename T> Ele<T>* gather(MtList<T>& q, auto filt) noexcept {
    Ele<T> *head = nullptr;
    apply(q, filt, [&](auto* ele) {
        ele->next = head;
        head = ele;
    });
    return head;
}

template<typename T>
void atomic_swap(MtList<T>& f, MtList<T>& s) noexcept {
    Ele<T> *fcurr = &f.trampoline;
    Ele<T> *fprev = fcurr;
    Ele<T> *scurr = &s.trampoline;
    Ele<T> *sprev = scurr;
    while ((fcurr = fcurr->next.exchange(fcurr, consume)) == fprev) {
        continue;
    }
    while ((scurr = scurr->next.exchange(scurr, consume)) == sprev) {
        continue;
    }
    fcurr->next.store(scurr, relaxed);
    scurr->next.store(fcurr, relaxed);
}

}
