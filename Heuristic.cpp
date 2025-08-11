#include "Heuristic.h"
#include <iostream>
#include <iterator>
#include <cmath>
#include <map>
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
int Heuristic::pickJeroslowWangVar(const std::vector<int>& assignment) {
    int bestVar = -1;
    double maxScore = -1.0;
    int numVars = static_cast<int>(assignment.size() - 1);

    for (int i = 1; i <= numVars; ++i) {
        if (assignment[i] == -1) { // Nur unzugewiesene Variablen prüfen
            if (jwScores[i] > maxScore) {
                maxScore = jwScores[i];
                bestVar = i;
            }
        }
    }

    if (bestVar == -1) {
        return pickRandomVar();
    }

    return bestVar;
}

// Initialisiert die Scores einmalig
void Heuristic::initializeJeroslowWang(const std::vector<Clause>& clauses, int numVars) {
    jwScores.assign(numVars + 1, 0.0);
    for (const auto& clause : clauses) {
        double weight = std::pow(2.0, -static_cast<double>(clause.getClause().size()));
        for (const auto& lit : clause.getClause()) {
            if (lit.getVar() < static_cast<int>(jwScores.size())) {
                jwScores[lit.getVar()] += weight;
            }
        }
    }
}

// Aktualisiert die Scores für eine einzelne neue Klausel
void Heuristic::updateJeroslowWang(const Clause& newClause) {
    double weight = std::pow(2.0, -static_cast<double>(newClause.getClause().size()));
    for (const auto& lit : newClause.getClause()) {
        if (lit.getVar() < static_cast<int>(jwScores.size())) {
            jwScores[lit.getVar()] += weight;
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