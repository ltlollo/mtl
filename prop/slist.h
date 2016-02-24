template <typename T> struct alignas(cacheln) Ele {
    std::atomic<Ele<T> *> next;
    T data;
    Ele() noexcept { next = nullptr; }
    Ele(T &&value) noexcept : data{std::move(value)} {
        static_assert(std::is_nothrow_move_constructible<T>(),
                      "move cannot throw");
        static_assert(std::is_default_constructible<T>(),
                      "must be default constructable");
    }
    Ele(const T &value) noexcept : data{value} {
        static_assert(std::is_nothrow_copy_constructible<T>(),
                      "copy cannot throw");
    }
};
template <typename T> struct alignas(cacheln) Ele<T *> {
    std::atomic<Ele<T *> *> next;
    T *data;
    Ele() noexcept {
        data = nullptr;
        next = nullptr;
    }
    Ele(T *value) noexcept : data{value} {}
    ~Ele() noexcept { delete data; }
};
template <typename T, unsigned N = 1> struct MtList {
    Ele<T> entry[N];
    MtList() {
        static_assert(N > 0, "must have at least one entry");
        for (unsigned i = 0; i < N - 1; ++i) {
            entry[i].next = &entry[i + 1];
        }
        entry[N - 1].next = nullptr;
    }
};
template <typename T> void chain(MtList<T, 1> &q, Ele<T> *ele) noexcept {
    Ele<T> *curr = &q.entry[0];
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
template <typename T, typename P, typename F>
void trim(MtList<T, 1> &q, F filt, P pred, bool cont = true) noexcept {
    Ele<T> *curr = &q.entry[0];
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
template <typename T, typename P, typename F>
void trimzip(MtList<T, 1> &q, F filt, P pred, bool cont = true) noexcept {
    Ele<T> *curr = &q.entry[0];
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
template <typename T, typename P>
bool insert(MtList<T, 1> &q, Ele<T> *head, Ele<T> *tail, P pred) noexcept {
    Ele<T> *curr = &q.entry[0];
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
template <typename T, typename P>
bool insert(MtList<T, 1> &q, Ele<T> *ele, P pred) noexcept {
    return insert(q, ele, ele, pred);
}
template <typename T, typename P>
bool push(MtList<T, 1> &q, Ele<T> *head, Ele<T> *tail, P pred) noexcept {
    Ele<T> *curr = &q.entry[0];
    Ele<T> *prev = curr;
    Ele<T> *next;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    do {
        auto cond = pred(curr);
        if (unlikely(cond)) {
            tail->next.store(curr, relaxed);
            prev->next.store(head, release);
           return true;
        } else {
            if ((next = curr) == nullptr) {
                break;
            } else {
                prefetch(next->data);
            }
            while ((next = next->next.exchange(next, consume)) == curr) {
                continue;
            }
            prev->next.store(curr, relaxed);
            prev = curr;
            curr = next;
        }
    } while(true);
    prev->next.store(nullptr, relaxed);
    return false;
}
template <typename T, typename P>
bool push(MtList<T, 1> &q, Ele<T> *ele, P pred) noexcept {
    return push(q, ele, ele, pred);
}

template <typename T>
void push(MtList<T, 1> &q, Ele<T> *head, Ele<T> *tail) noexcept {
    Ele<T> *curr = &q.entry[0];
    Ele<T> *prev = curr;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    tail->next.store(curr, relaxed);
    prev->next.store(head, release);
}
template <typename T> void push(MtList<T, 1> &q, Ele<T> *ele) noexcept {
    push(q, ele, ele);
}
template <typename T, typename F> T *get(MtList<T *, 1> &q, F filt) noexcept {
    T *res = nullptr;
    trim(q, filt,
         [&](auto *ele) {
             std::swap(res, ele->data);
             delete ele;
         },
         false);
    return res;
}
template <typename T, typename F> T get(MtList<T, 1> &q, F filt) noexcept {
    T res = {};
    trim(q, filt,
         [&](auto *ele) {
             res = std::move(ele->data);
             delete ele;
         },
         false);
    return res;
}
template <typename T, typename F> size_t rm(MtList<T, 1> &q, F filt) noexcept {
    size_t n = 0;
    trim(q, filt, [&](auto *ele) {
        delete ele;
        ++n;
    });
    return n;
}
template <typename T> T last(MtList<T, 1> &q) noexcept {
    T res = {};
    trimzip(q, [](T, Ele<T> *nx) { return nx == nullptr; },
            [&](auto *ele) {
                res = std::move(ele->data);
                delete ele;
            },
            false);
    return res;
}
template <typename T> T *last(MtList<T *, 1> &q) noexcept {
    T *res = nullptr;
    trimzip(q, [](T, Ele<T> *nx) { return nx == nullptr; },
            [&](auto *ele) {
                std::swap(res, ele->data);
                delete ele;
            },
            false);
    return res;
}
template <typename T> bool rmlast(MtList<T, 1> &q) noexcept {
    Ele<T> *res = nullptr;
    trimzip(q, [](auto, auto *nx) { return nx == nullptr; },
            [&](auto *ele) { res = ele; }, false);
    if (res) {
        delete res;
        return true;
    }
    return false;
}
template <typename T, typename F>
Ele<T> *gather(MtList<T, 1> &q, F filt) noexcept {
    Ele<T> *head = nullptr;
    trim(q, filt, [&](auto *ele) {
        ele->next = head;
        head = ele;
    });
    return head;
}
template <typename T> Ele<T> *tail(MtList<T, 1> &q) noexcept {
    Ele<T> *curr = &q.entry[0];
    Ele<T> *prev = curr;
    Ele<T> *head;
    while ((curr = curr->next.exchange(curr, consume)) == prev) {
        continue;
    }
    q.entry[0].next.store(nullptr, relaxed);
    head = curr;
    prev = curr;
    if (curr == nullptr) {
        return nullptr;
    }
    do {
        while ((curr = curr->next.exchange(curr, consume)) == prev) {
            continue;
        }
        if (curr == nullptr) {
            prev->next.store(nullptr, relaxed);
            return head;
        }
        prev->next.store(curr, relaxed);
        prev = curr;
    } while (true);
}
}
