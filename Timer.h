//
// Created by Martin Egetemeyer on 15.08.25.
//

#ifndef CDCL_SOLVER_TIMER_H
#define CDCL_SOLVER_TIMER_H

#include <chrono>
#include <cstdint>

struct ScopedTimer {
    uint64_t& acc;
    const std::chrono::steady_clock::time_point t0;
    explicit ScopedTimer(uint64_t& a)
        : acc(a), t0(std::chrono::steady_clock::now()) {}
    ~ScopedTimer() noexcept {
        acc += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0
            ).count()
        );
    }
};


#endif //CDCL_SOLVER_TIMER_H