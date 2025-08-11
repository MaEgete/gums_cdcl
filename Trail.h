#ifndef TRAIL_H
#define TRAIL_H

#include <vector>
#include <ostream>
#include "Literal.h"
#include "Clause.h"

class Trail {
private:
    struct TrailEntry {
        Literal lit;     // assigned literal
        int     level;   // decision level
        int     reason_idx; // index in Solver::clauses; -1 = decision
    };
    std::vector<TrailEntry> trail;

public:
    // Zuweisung pushen
    void assign(const Literal& lit, int level, int reason_idx);

    // Abfragen
    bool isAssigned(int var) const;
    int  currentLevel() const;
    Literal& getLastLiteral();
    int  getLevelOfVar(int var) const;
    int  getReasonIndexOfVar(int var) const; // -1 wenn Entscheidung / nicht gefunden

    // Alles oberhalb Level entfernen, gibt die entfernten Variablen zurück (zum Unassignen)
    std::vector<int> popAboveLevel(int level);

    // Read‑only Zugriff (nur zum Iterieren/Debuggen)
    const std::vector<TrailEntry>& getTrail() const;

    friend std::ostream& operator<<(std::ostream& os, const Trail& t);
};

#endif // TRAIL_H