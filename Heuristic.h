#ifndef HEURISTIC_H
#define HEURISTIC_H

#include <set>
#include <random>
#include <vector>
#include "Trail.h"
#include "Clause.h"

class Heuristic {
private:
    std::set<int> unassignedVars;
    static inline std::mt19937_64 rng = std::mt19937_64(std::random_device{}());

    // Speichert die Scores für jede Variable. Index 0 ignorieren.
    std::vector<double> jwScores;

public:
    void initialize(int numVars);
    int  pickRandomVar();
    void update(const Trail& trail, int numVars);

    int  pickJeroslowWangVar(const std::vector<int>& assignment);
    // Initialisiert die Scores für Jeroslow-Wang (JW-TS)
    void initializeJeroslowWang(const std::vector<Clause>& clauses, int numVars);
    // Aktualisiert die Scores für eine einzelne neue Klausel
    void updateJeroslowWang(const Clause& newClause);
};

#endif // HEURISTIC_H