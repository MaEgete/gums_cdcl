#include "Solver.h"
#include <algorithm>
#include <iostream>

Solver::Solver(int n) : numVars(n), assignment(numVars + 1, -1) {
    heuristic.initialize(numVars);
    //currentHeuristic = HeuristicType::RANDOM;// Standard-Heuristik festlegen
    currentHeuristic = HeuristicType::JEROSLOW_WANG;
}

bool Solver::solve() {
    if (currentHeuristic == HeuristicType::JEROSLOW_WANG) {
        heuristic.initializeJeroslowWang(clauses, numVars);
    }

    while (true) {
        Clause* conflict = unitPropagation();
        if (conflict != nullptr) { // Wenn es einen Konflikt gibt
            if (decisionLevel == 0) return false; // UNSAT
            auto [learnedClause, backjumpLevel] = analyzeConflict(conflict);
            addClause(learnedClause);
            backtrackToLevel(backjumpLevel);
        }
        else if (allVariablesAssigned()) {
            return true;
        }
        else {
            Literal decision = pickBranchingVariable();
            decisionLevel++;
            assign(decision, decisionLevel, -1);
        }
    }
}

Clause* Solver::unitPropagation() {
    bool propagated;
    do {
        propagated = false;
        for (size_t i = 0; i < clauses.size(); ++i) {
            auto& cla = clauses[i];

            bool    satisfied     = false;
            int     unsetLits     = 0;
            Literal unassignedLit(0,false);

            for (const auto& lit : cla.getClause()) {
                int var = lit.getVar();
                int val = assignment[var];

                if ((val == 1 && !lit.isNegated()) || (val == 0 && lit.isNegated())) {
                    satisfied = true;
                    break;
                }
                if (val == -1) {
                    ++unsetLits;
                    unassignedLit = lit;
                }
            }

            if (satisfied) continue;
            if (unsetLits == 1) {
                assign(unassignedLit, decisionLevel, static_cast<int>(i));
                propagated = true;
            }
            if (unsetLits == 0) {
                return &cla;
            }
        }
    } while (propagated);
    return nullptr;
}

void Solver::assign(const Literal& lit, int level, int reason_idx) {
    trail.assign(lit, level, reason_idx);
    assignment[lit.getVar()] = lit.isNegated() ? 0 : 1;
}

std::pair<Clause,int> Solver::analyzeConflict(const Clause* conflict) {
    Clause learnedClause = *conflict;
    const int currentLevel = decisionLevel;

    auto countCurrentLevel = [&](){
        int c = 0;
        for (const auto& l : learnedClause.getClause())
            if (trail.getLevelOfVar(l.getVar()) == currentLevel) ++c;
        return c;
    };

    while (countCurrentLevel() > 1) {
        Literal resolveLit(0,false);
        for (auto it = trail.getTrail().rbegin(); it != trail.getTrail().rend(); ++it) {
            bool inLearned = std::any_of(learnedClause.getClause().begin(),
                                         learnedClause.getClause().end(),
                                         [&](const Literal& L){ return L.getVar() == it->lit.getVar(); });
            if (inLearned) { resolveLit = it->lit; break; }
        }

        int reason_idx = trail.getReasonIndexOfVar(resolveLit.getVar());
        const Clause* reason = (reason_idx >= 0 && reason_idx < static_cast<int>(clauses.size()))
                             ? &clauses[reason_idx] : nullptr;
        if (!reason) break;

        std::vector<Literal> newLits;
        auto litEquals = [](const Literal& a, const Literal& b){
            return a.getVar() == b.getVar() && a.isNegated() == b.isNegated();
        };

        for (const auto& l : learnedClause.getClause()) {
            if (l.getVar() == resolveLit.getVar() && l.isNegated() != resolveLit.isNegated()) continue;
            if (std::ranges::none_of(newLits, [&](const Literal& x){ return litEquals(x,l); }))
                newLits.push_back(l);
        }

        for (const auto& l : reason->getClause()) {
            if (l.getVar() == resolveLit.getVar() && l.isNegated() == resolveLit.isNegated()) continue;
            if (std::ranges::none_of(newLits, [&](const Literal& x){ return litEquals(x,l); }))
                newLits.push_back(l);
        }

        learnedClause = Clause(std::move(newLits));
    }

    int backjumpLevel = 0;
    for (const auto& l : learnedClause.getClause()) {
        int lvl = trail.getLevelOfVar(l.getVar());
        if (lvl != currentLevel) backjumpLevel = std::max(backjumpLevel, lvl);
    }
    return {learnedClause, backjumpLevel};
}

void Solver::backtrackToLevel(int level) {
    auto popped = trail.popAboveLevel(level);
    for (int var : popped) assignment[var] = -1;
    decisionLevel = level;
}

Literal Solver::pickBranchingVariable() {
    int var = -1;
    switch (currentHeuristic) {
        case HeuristicType::RANDOM:
            heuristic.update(trail, numVars);
            var = heuristic.pickRandomVar();
            break;
        case HeuristicType::JEROSLOW_WANG:
            var = heuristic.pickJeroslowWangVar(assignment);
            break;
    }

    return Literal{var, false};
}

void Solver::setHeuristic(HeuristicType type) {
    currentHeuristic = type;
}

bool Solver::allVariablesAssigned() const {
    for (int i = 1; i <= numVars; ++i)
        if (assignment[i] == -1) return false;
    return true;
}

void Solver::addClause(const Clause& clause) {
    clauses.push_back(clause);
    if (currentHeuristic == HeuristicType::JEROSLOW_WANG) {
        heuristic.updateJeroslowWang(clause);
    }
}

void Solver::printModel() const {
    for (int i = 1; i <= numVars; ++i) {
        std::cout << "x" << i << " = " << (assignment[i] == 1 ? "True" : "False") << "\n";
    }
}