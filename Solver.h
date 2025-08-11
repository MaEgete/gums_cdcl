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
    int                 numVars;
    std::vector<Clause> clauses;
    Trail               trail;
    int                 decisionLevel = 0;
    Heuristic           heuristic;
    HeuristicType       currentHeuristic;

    std::vector<int> assignment; // assignment[i] = -1 (unbelegt), 0 (false), 1 (true), Index 0 ignorieren

public:
    explicit Solver(int n);

    bool   solve();
    Clause* unitPropagation();

    void assign(const Literal& lit, int level, int reason_idx);
    std::pair<Clause,int> analyzeConflict(const Clause* conflict);

    void    backtrackToLevel(int level);
    Literal pickBranchingVariable();

    void setHeuristic(HeuristicType type);

    bool allVariablesAssigned() const;
    void addClause(const Clause& clause);
    void printModel() const;
};

#endif // SOLVER_H