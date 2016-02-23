#pragma once

namespace mtl {

template <unsigned N> struct Entry;

template <typename T, unsigned N, unsigned M>
void chain(MtList<T, N> &q, Entry<M>, Ele<T> *ele) noexcept {
    static_assert(M < N, "must be inside the entry array");
    Ele<T> *curr = &q.entry[M];
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
template <typename T, typename P, typename F, unsigned N, unsigned M>
void trim(MtList<T, N> &q, Entry<M>, F filt, P pred,
          bool cont = true) noexcept {
    static_assert(M < N, "must be inside the entry array");
    Ele<T> *curr = &q.entry[M];
    Ele<T> *prev = curr;
    Ele<T> *next;
    bool cond;
    unsigned i = M;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        if (unlikely(curr == q.entry + i + 1)) {
            cond = false;
            ++i;
        } else {
            cond = filt(curr->data);
        }
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
template <typename T, typename P, typename F, unsigned N, unsigned M>
void trimzip(MtList<T, N> &q, Entry<M>, F filt, P pred,
             bool cont = true) noexcept {
    static_assert(M < N, "must be inside the entry array");
    Ele<T> *curr = &q.entry[M];
    Ele<T> *prev = curr;
    Ele<T> *next;
    bool cond;
    unsigned i = M;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        if (unlikely(curr == q.entry + i + 1)) {
            cond = false;
            ++i;
        } else {
            cond = filt(curr->data);
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

template <typename T, unsigned N>
Ele<T> *chunk(MtList<T, N> &q, unsigned m = 0) {
    if (m > N - 1) {
        m = 0;
    }
    Ele<T> *curr = &q.entry[m];
    Ele<T> *prev = curr;
    Ele<T> *head;
    unsigned i = m;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    Ele<T> *nxentry = (i == N - 1) ? nullptr : &q.entry[i + 1];
    q.entry[i].next.store(nxentry, relaxed);
    head = curr;
    prev = curr;
    if (curr == nxentry) {
        return nullptr;
    }
    do {
        while ((curr = curr->next.exchange(curr, consume)) == prev) {
            continue;
        }
        if (curr == nxentry) {
            prev->next.store(nullptr, relaxed);
            return head;
        }
        prev->next.store(curr, relaxed);
        prev = curr;
    } while (true);
}

template <typename T, typename P, unsigned N, unsigned M>
bool insert(MtList<T, N> &q, Entry<M>, Ele<T> *head, Ele<T> *tail,
            P pred) noexcept {
    static_assert(M < N, "must be inside the entry array");
    Ele<T> *curr = &q.entry[M];
    Ele<T> *prev = curr;
    Ele<T> *next;
    bool cond;
    unsigned i = M;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        if (unlikely(curr == q.entry + i + 1)) {
            cond = false;
            ++i;
        } else {
            cond = filt(prev, curr);
        }
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

template <typename T, typename P, unsigned N, unsigned M>
bool insert(MtList<T, N> &q, Entry<M> e, Ele<T> *ele, P pred) noexcept {
    return insert(q, e, ele, ele, pred);
}

template <typename T, unsigned N, unsigned M>
void push(MtList<T, N> &q, Entry<M>, Ele<T> *head, Ele<T> *tail) noexcept {
    static_assert(M < N, "must be inside the entry array");
    Ele<T> *curr = &q.entry[M];
    Ele<T> *prev = curr;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    tail->next.store(curr, relaxed);
    prev->next.store(head, release);
}
template <typename T, unsigned N, unsigned M>
void push(MtList<T, N> &q, Entry<M> e, Ele<T> *ele) noexcept {
    push(q, e, ele, ele);
}
template <typename T, typename F, unsigned N, unsigned M>
auto get(MtList<T *, N> &q, Entry<M> e, F filt) noexcept {
    T *res = nullptr;
    trim(q, e, filt,
         [&](auto *ele) {
             std::swap(res, ele->data);
             delete ele;
         },
         false);
    return res;
}
template <typename T, typename F, unsigned N, unsigned M>
T get(MtList<T, N> &q, Entry<M> e, F filt) noexcept {
    T res = {};
    trim(q, e, filt,
         [&](auto *ele) {
             res = std::move(ele->data);
             delete ele;
         },
         false);
    return res;
}
template <typename T, typename F, unsigned N, unsigned M>
size_t rm(MtList<T, N> &q, Entry<M> e, F filt) noexcept {
    size_t n = 0;
    trim(q, e, filt, [&](auto *ele) {
        delete ele;
        ++n;
    });
    return n;
}
template <typename T, unsigned N, unsigned M>
T last(MtList<T, N> &q, Entry<M> e = Entry<N - 1>()) noexcept {
    T res = {};
    trimzip(q, e, [](T, Ele<T> *nx) { return nx == nullptr; },
            [&](auto *ele) {
                res = std::move(ele->data);
                delete ele;
            },
            false);
    return res;
}
template <typename T, unsigned N, unsigned M>
T *last(MtList<T *, N> &q, Entry<M> e = Entry<N - 1>()) noexcept {
    T *res = nullptr;
    trimzip(q, e, [](T, Ele<T> *nx) { return nx == nullptr; },
            [&](auto *ele) {
                std::swap(res, ele->data);
                delete ele;
            },
            false);
    return res;
}
template <typename T, unsigned N, unsigned M>
bool rmlast(MtList<T, N> &q, Entry<M> e = Entry<N - 1>()) noexcept {
    Ele<T> *res = nullptr;
    trimzip(q, e, [](auto, auto *nx) { return nx == nullptr; },
            [&](auto *ele) { res = ele; }, false);
    if (res) {
        delete res;
        return true;
    }
    return false;
}
template <typename T, typename F, unsigned N, unsigned M>
Ele<T> *gather(MtList<T, N> &q, Entry<M> e, F filt) noexcept {
    Ele<T> *head = nullptr;
    trim(q, e, filt, [&](auto *ele) {
        ele->next = head;
        head = ele;
    });
    return head;
}

// non static interface

template <typename T, unsigned N>
void chain(MtList<T, N> &q, unsigned m, Ele<T> *ele) noexcept {
    if (m > N - 1) {
        m = 0;
    }
    Ele<T> *curr = &q.entry[m];
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
template <typename T, typename P, typename F, unsigned N>
void trim(MtList<T, N> &q, unsigned m, F filt, P pred,
          bool cont = true) noexcept {
    if (m > N - 1) {
        m = 0;
    }
    Ele<T> *curr = &q.entry[m];
    Ele<T> *prev = curr;
    Ele<T> *next;
    bool cond;
    unsigned i = m;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        if (unlikely(curr == q.entry + i + 1)) {
            cond = false;
            ++i;
        } else {
            cond = filt(curr->data);
        }
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
template <typename T, typename P, typename F, unsigned N>
void trimzip(MtList<T, N> &q, unsigned m, F filt, P pred,
             bool cont = true) noexcept {
    if (m > N - 1) {
        m = 0;
    }
    Ele<T> *curr = &q.entry[m];
    Ele<T> *prev = curr;
    Ele<T> *next;
    bool cond;
    unsigned i = m;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        while ((next = next->next.exchange(next, consume)) == curr) {
            continue;
        }
        if (unlikely(curr == q.entry + i + 1)) {
            cond = false;
            ++i;
        } else {
            cond = filt(curr->data);
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
template <typename T, typename P, unsigned N>
bool insert(MtList<T, N> &q, unsigned m, Ele<T> *head, Ele<T> *tail,
            P pred) noexcept {
    if (m > N - 1) {
        m = 0;
    }
    Ele<T> *curr = &q.entry[m];
    Ele<T> *prev = curr;
    Ele<T> *next;
    bool cond;
    unsigned i = m;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    while (likely(curr)) {
        next = curr;
        if (unlikely(curr == q.entry + i + 1)) {
            cond = false;
            ++i;
        } else {
            cond = filt(prev, curr);
        }
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

template <typename T, typename P, unsigned N>
bool insert(MtList<T, N> &q, unsigned m, Ele<T> *ele, P pred) noexcept {
    return insert(q, m, ele, ele, pred);
}

template <typename T, unsigned N>
void push(MtList<T, N> &q, unsigned m, Ele<T> *head, Ele<T> *tail) noexcept {
    if (m > N - 1) {
        m = 0;
    }
    Ele<T> *curr = &q.entry[m];
    Ele<T> *prev = curr;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    tail->next.store(curr, relaxed);
    prev->next.store(head, release);
}
template <typename T, unsigned N>
void push(MtList<T, N> &q, unsigned m, Ele<T> *ele) noexcept {
    push(q, m, ele, ele);
}
template <typename T, typename F, unsigned N>
T *get(MtList<T *, N> &q, unsigned m, F filt) noexcept {
    T *res = nullptr;
    trim(q, m, filt,
         [&](auto *ele) {
             std::swap(res, ele->data);
             delete ele;
         },
         false);
    return res;
}
template <typename T, typename F, unsigned N>
T get(MtList<T, N> &q, unsigned m, F filt) noexcept {
    T res = {};
    trim(q, m, filt,
         [&](auto *ele) {
             res = std::move(ele->data);
             delete ele;
         },
         false);
    return res;
}
template <typename T, typename F, unsigned N>
size_t rm(MtList<T, N> &q, unsigned m, F filt) noexcept {
    size_t n = 0;
    trim(q, m, filt, [&](auto *ele) {
        delete ele;
        ++n;
    });
    return n;
}
template <typename T, unsigned N>
T last(MtList<T, N> &q, unsigned m = N - 1) noexcept {
    T res = {};
    trimzip(q, m, [](T, Ele<T> *nx) { return nx == nullptr; },
            [&](auto *ele) {
                res = std::move(ele->data);
                delete ele;
            },
            false);
    return res;
}
template <typename T, unsigned N>
T *last(MtList<T *, N> &q, unsigned m = N - 1) noexcept {
    T *res = nullptr;
    trimzip(q, m, [](T, Ele<T> *nx) { return nx == nullptr; },
            [&](auto *ele) {
                std::swap(res, ele->data);
                delete ele;
            },
            false);
    return res;
}
template <typename T, unsigned N>
bool rmlast(MtList<T, N> &q, unsigned m = N - 1) noexcept {
    Ele<T> *res = nullptr;
    trimzip(q, m, [](auto, auto *nx) { return nx == nullptr; },
            [&](auto *ele) { res = ele; }, false);
    if (res) {
        delete res;
        return true;
    }
    return false;
}
template <typename T, typename F, unsigned N>
Ele<T> *gather(MtList<T, N> &q, unsigned m, F filt) noexcept {
    Ele<T> *head = nullptr;
    trim(q, m, filt, [&](auto *ele) {
        ele->next = head;
        head = ele;
    });
    return head;
}
}
