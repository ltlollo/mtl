#pragma once

#include <type_traits>
#include <atomic>
#include <utility>
#include <cstddef>

namespace mtl {

// Teseted functions are above this comment.

// Element of the `MtList`, contains `T data`, the user provided data, and
// `*next`, an atomic pointer to either the next element or nullptr.
// Restrictions: owned data must be noexcept movable, or copyable.
template <typename T> struct Ele;
// The `T *` specializetion considers the contained pointer owned, so that the
// class of functions dedicated to removal will automatically delete it.
template <typename T> struct Ele<T *>;

// Lock-free list, with `N` insertion points, `N` is 1 by default.
// Notes: no destructor is implemented.
//        prefer `N = 1` specialization.
template <typename T, unsigned N> struct MtList;

// Tail insetion function, will insert the `e` provided list at the end of
// the list.
// Notes: `e` must be a nullptr terminated list.
//        prefer other insertion methods.
template <typename T, unsigned N>
void chain(MtList<T, N> &, Ele<T> *e) noexcept;

// Utility function, removes elements if owned data matches `filt`,
// consequently applies `pred` to them. Returns immediatly if `cont` is set to
// false.
// Notes: `cont` is `true` by default
template <typename T, typename P, typename F, unsigned N>
void trim(MtList<T, N> &, F filt, P pred, bool cont) noexcept;
// same as `trim`, but `filt` will be applied to the current element's data,
// and the pointer to the next element.
// Notes: the next pointer applied to `filt` might be null.
template <typename T, typename P, typename F, unsigned N>
void trimzip(MtList<T, N> &, F, P, bool c) noexcept;

// Insertion function, inserts, the list linked between `head` and `tail`,
// after `pred` applied to an element matches.
// Notes: the list between `head` and `tail` must be valid.
template <typename T, typename P, unsigned N>
bool insert(MtList<T, N> &, Ele<T> *head, Ele<T> *tail, P pred) noexcept;
// Inserts just one element.
template <typename T, typename P, unsigned N>
bool insert(MtList<T, N> &, Ele<T> *, P) noexcept;

// Insertion function, inserts, the list linked between `head` and `tail`,
// before `pred` applied to an element pointer matches.
// Notes: the list between `head` and `tail` must be valid.
//        the element pointer might be null.
template <typename T, typename P, unsigned N>
bool push(MtList<T, N> &q, Ele<T> *head, Ele<T> *tail, P pred) noexcept;
// Inserts just one element.
template <typename T, typename P, unsigned N>
bool push(MtList<T, N> &q, Ele<T> *ele, P pred) noexcept;
// Inserts at the front.
template <typename T, unsigned N>
void push(MtList<T, N> &, Ele<T> *, Ele<T> *) noexcept;
template <typename T, unsigned N> void push(MtList<T, N> &, Ele<T> *) noexcept;

// Retrieval function, moves out of the list either the first data matching
// `pred`, or returns the default constructed version.
// Notes: std::move is called on the data.
template <typename T, typename F, unsigned N>
T get(MtList<T, N> &, F) noexcept;
// Notes: if the `T *` if the data is not found nullptr, is returned
template <typename T, typename F, unsigned N>
T *get(MtList<T *, N> &, F pred) noexcept;

// Removal function, removes the data matching `pred`.
// Returnes the number of elements removed this way.
template <typename T, typename F, unsigned N>
size_t rm(MtList<T, N> &, F) noexcept;
// Deletes the data.
template <typename T, typename F, unsigned N>
size_t rm(MtList<T *, N> &, F) noexcept;

// Retrieval function, moves out of the list either the last element's data, if
// any, or returns the default constructed version.
// Notes: prefer other retrieval functions.
template <typename T, unsigned N> T last(MtList<T, N> &) noexcept;
// Notes: if the list is empty nullptr, is returned.
template <typename T, unsigned N> T *last(MtList<T *, N> &) noexcept;

// Removal function, removes the last element of the list, if any, in case
// returning true.
// Notes: prefer other removal functions.
template <typename T, unsigned N> bool rmlast(MtList<T, N> &) noexcept;
// Deletes the data.
template <typename T, unsigned N> bool rmlast(MtList<T *, N> &) noexcept;

// Retrieval function, constructs a reversed list of the elements' data
// matching `pred` and returns the pointer to the first element.
// Notes: if no data matches, returns nullptr.
template <typename T, typename F, unsigned N>
Ele<T> *gather(MtList<T, N> &, F) noexcept;

// Retrieval function, gets the entire list, if any.
// Notes: if the list is empty returns nullptr.
template <typename T, unsigned N> Ele<T> *tail(MtList<T, N> &) noexcept;

static constexpr auto cacheln = 64;
static constexpr auto consume = std::memory_order_consume;
static constexpr auto relaxed = std::memory_order_relaxed;
static constexpr auto release = std::memory_order_release;
#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x)   __builtin_expect(!!(x), 1)
template<typename T> void prefetch(T) {}
template<typename T> void prefetch(T* x) { __buildin_prefetch(x); }

#include "slist.h"
#include "mlist.h"
