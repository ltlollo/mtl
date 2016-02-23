#pragma once

#include <atomic>
#include <utility>
#include <cstddef>
#include <stdlib.h>
#include "com.h"

static constexpr auto cacheln = 64;
static constexpr auto consume = std::memory_order_consume;
static constexpr auto relaxed = std::memory_order_relaxed;
static constexpr auto release = std::memory_order_release;
#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x)   __builtin_expect(!!(x), 1)

namespace mtl {

template<NoOwner T> void prefetch(T&) {
}
template<Owner T> void prefetch(T& x) {
    __builtin_prefetch(x.data);
}

template<typename T> struct alignas(cacheln) Ele {
    using Owned = T;
    std::atomic<Ele<T>*> next;
    T data;
};
void init(Ele<Init>& e) {
    e.next = nullptr;
    init(e.data);
}
void init(Ele<NoInit>& e) {
    e.next = nullptr;
}
void del(Ele<Del>& ele) {
    del(ele.data);
    free(&ele);
}
void del(Ele<NoDel>& ele) {
    free(&ele);
}

template<typename T> struct MtList {
    Ele<T> trampoline;
};
void init(MtList<auto>& q) {
    init(q.trampoline);
}
template<typename T> void chain(MtList<T>& q, Ele<T>* ele) noexcept {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    Ele<T> *next;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        prev->next.store(curr, relaxed);
        prev = curr;
        curr = next;
    }
    prev->next.store(ele, relaxed);
    return;
}
template<typename T, typename P, typename F>
void trim(MtList<T>& q, F filt, P pred, bool cont = true) {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    Ele<T> *next;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        auto cond = filt(curr->data);
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        if (unlikely(cond)) {
            pred(curr);
            if (!cont) {
                prev->next.store(next, relaxed);
                return;
            }
            curr = next;
        } else {
            if (likely(next)) {
                prefetch(next->data);
            }
            prev->next.store(curr, relaxed);
            prev = curr;
            curr = next;
        }
    }
    prev->next.store(nullptr, relaxed);
}
template<typename T, typename P, typename F>
void trimzip(MtList<T>& q, F filt, P pred, bool cont = true) {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    Ele<T> *next;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        auto cond = filt(curr->data, next);
        if (unlikely(cond)) {
            pred(curr);
            if (!cont) {
                prev->next.store(next, relaxed);
                return;
            }
            curr = next;
        } else {
            if (likely(next)) {
                prefetch(next->data);
            }
            prev->next.store(curr, relaxed);
            prev = curr;
            curr = next;
        }
    }
    prev->next.store(nullptr, relaxed);
}
template<typename T, typename P>
bool insert(MtList<T>& q, Ele<T>* head, Ele<T>* tail, P pred) {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    Ele<T> *next;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        auto cond = pred(prev, curr);
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        if (unlikely(cond)) {
            tail->next.store(next, relaxed);
            curr->next.store(head, release);
            return true;
        } else {
            if (likely(next)) {
                prefetch(next->data);
            }
            prev->next.store(curr, relaxed);
            prev = curr;
            curr = next;
        }
    }
    prev->next.store(nullptr, relaxed);
    return false;
}

template<typename T, typename P>
bool insert(MtList<T>& q, Ele<T>* ele, P pred) {
    return insert(q, ele, ele, pred);
}

template<typename T>
void push(MtList<T>& q, Ele<T>* head, Ele<T>* tail) {
    Ele<T> *curr = &q.trampoline;
    Ele<T> *prev = curr;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    tail->next.store(curr, relaxed);
    prev->next.store(head, release);
}
template<typename T>
void push(MtList<T>& q, Ele<T>* ele) {
    push(q, ele, ele);
}
template<typename T, typename F>
auto get(MtList<T>& q, F filt) {
    T res;
    init(res);
    trim(q, filt, [&](auto* ele) {
          res = ele->data;
          free(ele);
    }, false);
    return res;
}
template<typename T, typename F>
size_t rm(MtList<T>& q, F filt) {
    size_t n = 0;
    trim(q, filt, [&](auto* ele) {
         del(*ele);
          ++n;
    });
    return n;
}
template<Init T>
auto last(MtList<T>& q) {
    T res;
    init(res);
    trimzip(q, [](T, Ele<T>* nx) {
          return nx == nullptr;
    }, [&](auto* ele) {
          res = ele->data;
          free(ele);
    }, false);
    return res;
}
template<typename T>
bool rmlast(MtList<T>& q) {
    Ele<T> *res = nullptr;
    trimzip(q, [](auto, auto* nx) {
        return nx == nullptr;
    }, [&](auto* ele) {
        res = ele;
    }, false);
    if (res) {
        del(*res);
        return true;
    }
    return false;
}
template<typename T, typename F>
Ele<T>* gather(MtList<T>& q, F filt) {
    Ele<T> *head = nullptr;
    trim(q, filt, [&](auto* ele) {
        ele->next = head;
        head = ele;
    });
    return head;
}

template<typename T>
void atomic_swap(MtList<T>& f, MtList<T>& s) {
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
