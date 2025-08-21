#ifndef SOLVER_H
#define SOLVER_H

#include <vector>
#include "Clause.h"
#include "Trail.h"
#include "Heuristic.h"

struct Stats {
    uint64_t decisions=0, conflicts=0, propagations=0;
    uint64_t learnts_added=0, restarts=0;
    uint64_t clause_inspections=0, watch_moves=0;
    uint64_t t_bcp_ms=0, t_analyze_ms=0;

    // --- LBD statistics ---
    uint64_t learnt_lbd_sum   = 0;  // Summe der LBDs gelernter Klauseln
    uint64_t learnt_lbd_count = 0;  // Anzahl gelernter Klauseln (für die LBD berechnet wurde)
    uint64_t learnt_lbd_le2   = 0;  // #gelernter Klauseln mit LBD <= 2
    uint64_t learnt_lbd_3_4   = 0;  // #gelernter Klauseln mit LBD in {3,4}
    uint64_t learnt_lbd_ge5   = 0;  // #gelernter Klauseln mit LBD >= 5

    uint64_t deleted_count    = 0;  // #gelöschter Klauseln in reduceDB
    uint64_t deleted_lbd_sum  = 0;  // Summe der LBDs gelöschter Klauseln
};

enum class HeuristicType {
    RANDOM,
    JEROSLOW_WANG
};



class Solver {
private:

    Stats stats;

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
    std::vector<int> savedPhase; // -1 = unknown, 0 = prefer false (negated), 1 = prefer true (non-negated)

    int restart_idx = 1;
    int restart_base = 2;
    int conflicts_since_restart = 0;
    int restart_budget = 0;

    double clauseInc = 1.0;
    double clauseDecay = 0.999;


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
    void printStats() const;

    Clause* propagate();             // neue Propagation mit 2WL
    void    attachExistingClauses(); // falls Clauses schon im Vektor sind
    void    seedRootUnits();         // root-level Units vorab enqueuen

    void reduceDB();

    void setHeuristicSeed(uint64_t s);

    void bumpClauseActivity(Clause& c);
    void decayClauseInc();

    static std::string heuristicToString(HeuristicType type) ;
};

#endif // SOLVER_H