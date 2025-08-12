#ifndef SOLVER_H
#define SOLVER_H

#include <vector>
#include "Clause.h"
#include "Trail.h"
#include "Heuristic.h"

enum class HeuristicType {
    RANDOM,
    JEROSLOW_WANG
};

class Solver {
private:
    int numVars;
    // Für jedes Literal (inkl. Negation) eine Liste beobachtender Klauseln (Indices)
    std::vector<std::vector<size_t>> watchList;
    size_t qhead = 0; // Index ins Trail: bis wohin wurde propagiert?

    int     litToIndex(const Literal& l) const; // mappe Literal -> [0 .. 2*numVars-1]
    Literal negate(const Literal& l) const;

    void    attachClause(size_t clauseIdx, const Literal& w);
    void    detachClause(size_t clauseIdx, const Literal& w);
    Clause* propagateLiteralFalse(const Literal& falsified);

    std::vector<Clause> clauses;
    Trail               trail;
    int                 decisionLevel = 0;
    Heuristic           heuristic;
    HeuristicType       currentHeuristic;

    // assignment[i] = -1 (unbelegt), 0 (false), 1 (true), Index 0 ignorieren
    std::vector<int> assignment;

public:
    explicit Solver(int n);

    bool     solve();
    Clause*  unitPropagation(); // alte O(n*m)-Variante (optional)

    void assign(const Literal& lit, int level, int reason_idx);
    std::tuple<Clause,int,Literal> analyzeConflict(const Clause* conflict);

    void    backtrackToLevel(int level);
    Literal pickBranchingVariable();

    void setHeuristic(HeuristicType type);

    bool allVariablesAssigned() const;
    void addClause(const Clause& clause);
    void printModel() const;

    Clause* propagate();             // neue Propagation mit 2WL
    void    attachExistingClauses(); // falls Clauses schon im Vektor sind
    void    seedRootUnits();         // root-level Units vorab enqueuen
};

#endif // SOLVER_H