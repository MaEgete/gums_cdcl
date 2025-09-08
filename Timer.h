

#ifndef CDCL_SOLVER_TIMER_H
#define CDCL_SOLVER_TIMER_H

#include <chrono>
#include <cstdint>

// Misst die Zeitspanne zwischen Konstruktor- und Destruktoraufruf
// und addiert sie zu einer Referenzvariablen
struct ScopedTimer {
    uint64_t& acc; // Zielvariable, in die die gemessene Zeit addiert wird
    const std::chrono::steady_clock::time_point t0; // Startzeit

    // Konstruktor: startet den Timer und merkt sich Referenz auf die Zielvariable
    explicit ScopedTimer(uint64_t& a)
        : acc(a), t0(std::chrono::steady_clock::now()) {}

    // Destruktor: wird beim Verlassen des Scopes aufgerufen,
    // berechnet die verstrichene Zeit in Millisekunden und addiert sie zu acc
    ~ScopedTimer() noexcept {
        acc += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0
            ).count()
        );
    }
};

#endif // CDCL_SOLVER_TIMER_H