#include "Heuristic.h"
#include <iterator>
#include <cmath>
#include <stdexcept>

// Ursprüngliche Zufalls-Heuristik
int Heuristic::pickRandomVar() {
    if (unassignedVars.empty()) {
        throw std::runtime_error("Kann keine Variable von einem leeren Set nehmen");
    }
    std::uniform_int_distribution<size_t> dist(0, unassignedVars.size() - 1);
    size_t randomIndex = dist(rng);

    auto it = unassignedVars.begin();
    std::advance(it, randomIndex);
    return *it;
}

// Jeroslow-Wang-Heuristik (jetzt basierend auf den internen Scores)
std::pair<int, bool> Heuristic::pickJeroslowWangVar(const std::vector<int>& assignment) const {
    int bestVar  = -1;
    double bestSum  = -1.0;
    double bestMax = -1.0;
    double bestPos = -1.0;
    bool bestNeg = false;

    int numVars  = static_cast<int>(assignment.size() - 1);

    for (int v = 1; v <= numVars; ++v) {
        if (assignment[v] != -1) {
            continue;
        }

        const double pos = jwPosScores[v];
        const double neg = jwNegScores[v];
        const double sum = pos + neg;
        const double mx = (pos > neg ? pos : neg);

        if (sum > bestSum // größere Summe gewinnt
            || (sum == bestSum && mx > bestMax) // bei gleicher Summe: stärkere Seite größer
            || (sum == bestSum && mx == bestMax && pos > bestPos) // noch gleich: höheres pos bevorzugen
            || (sum == bestSum && mx == bestMax && pos == bestPos && (bestVar == -1 || v < bestVar))){ // immer noch gleich: kleinerer Index

            bestSum = sum; // neue führende Summe
            bestMax = mx; // stärkere Seite des Champions
            bestPos = pos; // pos-Score des Champions
            bestVar = v; // Champion-Variable
            bestNeg = (neg > pos); // Polarität auf stärkere Seite
            }
    }

    // Fallback: erste unbelegte Var + sinnvolle Polarität
    if (bestVar == -1) {
        int v = -1;
        for (int i = 1; i <= numVars; ++i) {
            if (assignment[i] == -1) {
                v = i; break;
            }
        }
        if (v == -1) {
            return {-1, false}; // alles belegt (sollte nicht vorkommen)
        }

        const bool negPol = (jwNegScores[v] > jwPosScores[v]); // sonst positiv
        return {v, negPol};
    }

    return {bestVar, bestNeg};
}



// Initialisiert die Scores einmalig
void Heuristic::initializeJeroslowWang(const std::vector<Clause>& clauses, int numVars) {
    jwPosScores.assign(numVars + 1, 0.0);
    jwNegScores.assign(numVars + 1, 0.0);

    for (const auto& clause : clauses) {
        double weight = std::pow(2.0, -static_cast<double>(clause.getClause().size()));
        for (const auto& lit : clause.getClause()) {
            if (lit.getVar() >= 1 && lit.getVar() < static_cast<int>(jwPosScores.size())) {
                if (lit.isNegated()) {
                    jwNegScores[lit.getVar()] += weight;
                }
                else {
                    jwPosScores[lit.getVar()] += weight;
                }
            }
        }
    }
}

// Aktualisiert die Scores für eine einzelne neue Klausel
void Heuristic::updateJeroslowWang(const Clause& newClause) {
    double weight = std::pow(2.0, -static_cast<double>(newClause.getClause().size()));
    for (const auto& lit : newClause.getClause()) {
        if (lit.getVar() >= 1 && lit.getVar() < static_cast<int>(jwPosScores.size())) {
            if (lit.isNegated()) {
                jwNegScores[lit.getVar()] += weight;
            }
            else {
                jwPosScores[lit.getVar()] += weight;
            }
        }
    }
}

void Heuristic::update(const Trail& trail, int numVars) {
    unassignedVars.clear();
    for (int var = 1; var <= numVars; ++var) {
        if (!trail.isAssigned(var)) {
            unassignedVars.insert(var);
        }
    }
}

void Heuristic::initialize(int numVars) {
    unassignedVars.clear();
    for (int v = 1; v <= numVars; ++v) unassignedVars.insert(v);
}

void Heuristic::setSeed(uint64_t s) {
    rng.seed(s);
}
