#include <cstdint>
#include <chrono>
#include <stdio.h>
#include <atomic>

namespace mtl {

struct alignas(64) Count {
    std::atomic<size_t> data;
};

static Count counters[512];

template<typename P>
void bench(P pred, const char *msg, size_t times = 1000) {
    using namespace std::chrono;
    using ms = milliseconds;
    using clk = high_resolution_clock;
    auto beg = clk::now();
    for (unsigned i = 0; i < times; ++i) {
        pred();
        asm volatile("" : : : "memory");
    }
    auto end = clk::now();
    auto diff = duration_cast<ms>(end-beg).count();
    fprintf(stderr, "%s took: %ldms\n", msg, diff);
}

}
