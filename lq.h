#pragma once

#include "com.h"
#include <atomic>

static constexpr auto cacheln = 64;
static constexpr auto consume = std::memory_order_consume;
static constexpr auto relaxed = std::memory_order_relaxed;
static constexpr auto release = std::memory_order_release;

namespace mtl {

template<typename T> struct alignas(cacheln) Ele {
    std::atomic<Ele<T>*> next;
    T data;
};

template<typename T> struct Aq {
    Ele<T> trampoline;
};

template<typename T> void init(T*& d) {
    d = nullptr;
}
template<typename T> void init(std::atomic<T*>& d) {
    d = nullptr;
}
void init(Aq<auto>& q) {
    init(q.trampoline.next);
}
template<typename T> void del(T*& d) {
    free(d);
}

template<typename T> void chain(Aq<T>& q, Ele<T>* ele) {
    Ele<T>* curr = &q.trampoline;
    Ele<T>* prev = curr;
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
void apply(Aq<T>& q, auto filt, auto pred, bool cont = true) {
    Ele<T>* curr = &q.trampoline;
    Ele<T>* prev = curr;
    Ele<T>* next;
    do {
        while ((curr = curr->next.exchange(curr, consume)) == prev) {
            continue;
        }
        if (curr == nullptr) {
            prev->next.store(nullptr, relaxed);
            break;
        }
        next = curr;
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        if (filt(curr->data)) {
            pred(curr);
            prev->next.store(next, relaxed);
            if (!cont) {
                break;
            }
            curr = prev;
        } else {
            prev->next.store(curr, relaxed);
            curr->next.store(next, relaxed);
            prev = curr;
        }
    } while (true);
}

template<typename T>
void applyzip(Aq<T>& q, auto filt, auto pred, bool cont = true) {
    Ele<T>* curr = &q.trampoline;
    Ele<T>* prev = curr;
    Ele<T>* next;
    do {
        while ((curr = curr->next.exchange(curr, consume)) == prev) {
            continue;
        }
        if (curr == nullptr) {
            prev->next.store(nullptr, relaxed);
            break;
        }
        next = curr;
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        if (filt(curr->data, next)) {
            pred(curr);
            prev->next.store(next, relaxed);
            if (!cont) {
                break;
            }
            curr = prev;
        } else {
            prev->next.store(curr, relaxed);
            curr->next.store(next, relaxed);
            prev = curr;
        }
    } while (true);
}

template<typename T> bool insert(Aq<T>& q, auto* head, auto* tail, auto pred) {
    Ele<T>* curr = &q.trampoline;
    Ele<T>* prev = curr;
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

template<typename T> bool insert(Aq<T>& q, Ele<T>* ele, auto pred) {
    return insert(q, ele, ele, pred);
}

template<typename T> void push(Aq<T>& q, Ele<T>* head, Ele<T>* tail) {
    Ele<T>* curr = &q.trampoline;
    Ele<T>* prev = curr;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    tail->next.store(curr, relaxed);
    prev->next.store(head, release);
}

template<typename T> void push(Aq<T>& q, Ele<T>* ele) {
    push(q, ele, ele);
}

template<Del T> T get(Aq<T>& q, auto filt) {
    T res;
    init(res);
    apply(q, filt, [&](auto ele) {
          res = ele->data;
          del(ele);
    }, false);
    return res;
}
template<NoDel T> T get(Aq<T>& q, auto filt) {
    T res;
    init(res);
    apply(q, filt, [&](auto ele) {
          res = ele->data;
    }, false);
    return res;
}
size_t rm(Aq<auto>& q, auto filt) {
    size_t n = 0;
    apply(q, filt, [&](auto ele) {
          del(ele->data);
          del(ele);
          ++n;
    });
    return n;
}
size_t rm(Aq<Del>& q, auto filt) {
    size_t n = 0;
    apply(q, filt, [&](auto ele) {
          del(ele->data);
          del(ele);
          ++n;
    });
    return n;
}
size_t rm(Aq<NoDel>& q, auto filt) {
    size_t n = 0;
    apply(q, filt, [&](auto ele) {
          del(ele);
          ++n;
    });
    return n;
}
template<Del T> auto last(Aq<T>& q) {
    T res;
    init(res);
    applyzip(q, [](T, Ele<T>* nx) {
          return nx == nullptr;
    }, [&](auto ele) {
          res = ele->data;
          del(ele);
    }, false);
    return res;
}
template<NoDel T> auto last(Aq<T>& q) {
    T res;
    init(res);
    applyzip(q, [](T, Ele<T>* nx) {
          return nx == nullptr;
    }, [&](auto ele) {
          res = ele->data;
    }, false);
    return res;
}

template<Del T> bool rmlast(Aq<T>& q) {
    T ele = last(q);
    if (ele) {
        del(ele);
        return true;
    }
    return false;
}
bool rmlast(Aq<NoDel>& q) {
    return last(q);
}
template<typename T> Ele<T>* gather(Aq<T>& q, auto filt) {
    Ele<T> *head = nullptr;
    apply(q, filt, [&](auto* ele) {
        ele->next = head;
        head = ele;
    });
    return head;
}

}
