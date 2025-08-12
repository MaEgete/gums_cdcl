#include "Solver.h"
#include <algorithm>
#include <iostream>

Solver::Solver(int n) : numVars(n), assignment(numVars + 1, -1) {
    heuristic.initialize(numVars);
    //currentHeuristic = HeuristicType::RANDOM;
    currentHeuristic = HeuristicType::JEROSLOW_WANG;
    watchList.assign(2 * numVars, {}); // 2 Einträge je Variable (pos/neg)
}

bool Solver::solve() {
    // Optional: etwas Puffer für gelernte Klauseln
    if (!clauses.empty()) clauses.reserve(clauses.size() + 1024);

    attachExistingClauses();
    seedRootUnits();

    if (currentHeuristic == HeuristicType::JEROSLOW_WANG) {
        heuristic.initializeJeroslowWang(clauses, numVars);
    }

    while (true) {
        Clause* conflict = propagate();
        if (conflict != nullptr) {
            if (decisionLevel == 0) return false; // UNSAT auf Root-Level

            auto [learnedClause, backjumpLevel, assertLit] = analyzeConflict(conflict);
            backtrackToLevel(backjumpLevel);

            addClause(learnedClause);
            int reason_idx = static_cast<int>(clauses.size()) - 1;

            assign(assertLit, backjumpLevel, reason_idx);
            continue; // sofort weiter propagieren
        }
        else if (allVariablesAssigned()) {
            return true; // SAT
        }
        else {
            Literal decision = pickBranchingVariable();
            decisionLevel++;
            assign(decision, decisionLevel, -1); // decision (keine Reason)
        }
    }
}

int Solver::litToIndex(const Literal &l) const {
    // Variablen sind 1..numVars
    // Indexschema: pos(x) -> (var-1)*2, neg(x) -> (var-1)*2 + 1
    return (l.getVar() - 1) * 2 + (l.isNegated() ? 1 : 0);
}

Literal Solver::negate(const Literal &l) const {
    return Literal(l.getVar(), !l.isNegated());
}

void Solver::attachClause(size_t clauseIdx, const Literal &w) {
    watchList[litToIndex(w)].push_back(clauseIdx);
}

void Solver::detachClause(size_t clauseIdx, const Literal &w) {
    auto& v = watchList[litToIndex(w)];
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == clauseIdx) {
            // swap-remove
            v[i] = v.back();
            v.pop_back();
            return;
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
                return &cla; // Konflikt
            }
        }
    } while (propagated);
    return nullptr;
}

void Solver::assign(const Literal& lit, int level, int reason_idx) {
    trail.assign(lit, level, reason_idx); // an Trail anhängen (enqueue)
    assignment[lit.getVar()] = lit.isNegated() ? 0 : 1;
}

std::tuple<Clause,int,Literal> Solver::analyzeConflict(const Clause* conflict) {
    Clause learnedClause = *conflict;
    const int currentLevel = decisionLevel;

    auto countAtLevel = [&](int lvl) {
        int c = 0;
        for (const auto& l : learnedClause.getClause()) {
            if (trail.getLevelOfVar(l.getVar()) == lvl) ++c;
        }
        return c;
    };

    // Resolve bis 1-UIP: genau 1 Literal aus currentLevel bleibt
    while (countAtLevel(currentLevel) > 1) {
        // jüngstes Literal aus currentLevel in learnedClause finden
        Literal resolveLit(0,false);
        for (auto it = trail.getTrail().rbegin(); it != trail.getTrail().rend(); ++it) {
            if (trail.getLevelOfVar(it->lit.getVar()) != currentLevel) continue;
            bool inLearned = std::any_of(
                learnedClause.getClause().begin(), learnedClause.getClause().end(),
                [&](const Literal& L){ return L.getVar() == it->lit.getVar(); }
            );
            if (inLearned) { resolveLit = it->lit; break; }
        }

        // Reason holen
        int reason_idx = trail.getReasonIndexOfVar(resolveLit.getVar());
        const Clause* reason = (reason_idx >= 0 && reason_idx < static_cast<int>(clauses.size()))
                             ? &clauses[reason_idx] : nullptr;
        if (!reason) break; // defensiv

        // Auflösen über resolveLit
        std::vector<Literal> newLits;
        auto litEquals = [](const Literal& a, const Literal& b){
            return a.getVar() == b.getVar() && a.isNegated() == b.isNegated();
        };

        // learnedClause: Komplement zu resolveLit entfernen
        for (const auto& l : learnedClause.getClause()) {
            if (l.getVar() == resolveLit.getVar() && l.isNegated() != resolveLit.isNegated()) continue;
            if (std::ranges::none_of(newLits, [&](const Literal& x){ return litEquals(x,l); }))
                newLits.push_back(l);
        }
        // reason: gleiches Vorzeichen wie resolveLit entfernen
        for (const auto& l : reason->getClause()) {
            if (l.getVar() == resolveLit.getVar() && l.isNegated() == resolveLit.isNegated()) continue;
            if (std::ranges::none_of(newLits, [&](const Literal& x){ return litEquals(x,l); }))
                newLits.push_back(l);
        }

        learnedClause = Clause(std::move(newLits));
    }

    // assertierendes Literal = einziges Literal der Klausel auf currentLevel
    Literal assertLit(0, false);
    for (const auto& l: learnedClause.getClause()) {
        if (trail.getLevelOfVar(l.getVar()) == currentLevel) {
            assertLit = l;
            break;
        }
    }

    // Backjump-Level = max Level der übrigen Literale (≠ currentLevel)
    int backjumpLevel = 0;
    for (const auto& l : learnedClause.getClause()) {
        int lvl = trail.getLevelOfVar(l.getVar());
        if (lvl != currentLevel) backjumpLevel = std::max(backjumpLevel, lvl);
    }

    return {learnedClause, backjumpLevel, assertLit};
}

void Solver::backtrackToLevel(int level) {
    auto popped = trail.popAboveLevel(level);
    for (int var : popped) assignment[var] = -1;
    decisionLevel = level;

    // qhead darf nie größer als die aktuelle Trail-Länge sein
    if (qhead > trail.getTrail().size()) {
        qhead = trail.getTrail().size();
    }
}

void Solver::attachExistingClauses() {
    for (size_t idx = 0; idx < clauses.size(); ++idx){
        auto& c = clauses[idx];
        if (c.size() > 0 && (c.watch0() == -1 || c.watch1() == -1)) {
            c.initWatchesDefault();
        }
        if (c.size() == 1) {
            attachClause(idx, c.at(0)); // nur einmal attachen
        } else {
            if (c.watch0() != -1) attachClause(idx, c.at(c.watch0()));
            if (c.watch1() != -1) attachClause(idx, c.at(c.watch1()));
        }
    }
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

    // Guard: falls Heuristik nichts Sinnvolles liefert, nimm erstes unassigned
    if (var <= 0 || var > numVars || assignment[var] != -1) {
        for (int v = 1; v <= numVars; ++v) {
            if (assignment[v] == -1) { var = v; break; }
        }
        if (var <= 0) var = 1; // sollte durch allVariablesAssigned() nie relevant sein
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
    size_t idx = clauses.size() - 1;
    Clause& c = clauses[idx];

    if (c.size() > 0 && (c.watch0() == -1 || c.watch1() == -1))
        c.initWatchesDefault();

    if (c.size() == 1) {
        attachClause(idx, c.at(0));
    } else {
        if (c.watch0() != -1) attachClause(idx, c.at(c.watch0()));
        if (c.watch1() != -1) attachClause(idx, c.at(c.watch1()));
    }

    if (currentHeuristic == HeuristicType::JEROSLOW_WANG) {
        heuristic.updateJeroslowWang(c);
    }
}

void Solver::printModel() const {
    for (int i = 1; i <= numVars; ++i) {
        std::cout << "x" << i << " = " << (assignment[i] == 1 ? "True" : "False") << "\n";
    }
}

Clause* Solver::propagateLiteralFalse(const Literal &falsified) {
    // Nur Klauseln betrachten, die das aktuell falsifizierte Literal beobachten
    auto& wl = watchList[litToIndex(falsified)];
    size_t i = 0;
    while (i < wl.size()) {
        size_t cidx = wl[i];
        Clause* C = &clauses[cidx];

        const int w0 = C->watch0();
        const int w1 = C->watch1();

        // Welcher Watch zeigt gerade auf "falsified"?
        int wIdx = -1;
        if (w0 != -1 && C->at(w0) == falsified) wIdx = 0;
        else if (w1 != -1 && C->at(w1) == falsified) wIdx = 1;

        // Sicherheitsabfrage
        if (wIdx == -1) { ++i; continue; }

        // WICHTIGER FIX: otherIdx aus w0/w1 ermitteln, nicht aus wIdx (0/1)!
        const int otherIdx = (wIdx == 0 ? w1 : w0);

        const Literal& other = C->at(otherIdx);

        // Wenn der andere Watch schon wahr ist -> Klausel erfüllt
        if (assignment[other.getVar()] != -1) {
            const bool val = (assignment[other.getVar()] == 1);
            if (val != other.isNegated()) { ++i; continue; } // literal true
        }

        // Versuche, Watch auf ein anderes nicht-falsches Literal zu verschieben
        bool moved = false;
        for (size_t k = 0; k < C->size(); ++k) {
            if ((int)k == w0 || (int)k == w1) continue;
            const Literal& cand = C->at(k);

            // cand ist nicht-falsch? (unassigned oder wahr)
            int a = assignment[cand.getVar()];
            bool candFalse = (a != -1) && ( (a==1) == cand.isNegated() );

            if (!candFalse) {
                // Watch von "falsified" -> "cand" umhängen
                wl[i] = wl.back(); // swap-remove aus dieser Watchliste
                wl.pop_back();
                attachClause(cidx, cand); // in die neue Watchliste einhängen
                if (wIdx == 0) C->setWatch0((int)k); else C->setWatch1((int)k);
                moved = true;
                break;
            }
        }

        if (moved) {
            // Wichtig: wl[i] hat sich via detach/attach verändert → an Stelle i steht ein neues Element
            continue; // i NICHT erhöhen
        }

        // Kein Ersatz gefunden -> Unit oder Konflikt
        int a = assignment[other.getVar()];
        bool otherFalse = (a != -1) && ( (a==1) == other.isNegated() );

        if (otherFalse) {
            // beide Watches falsifiziert -> Konflikt
            return C;
        } else {
            // Unit: setze "other" mit Reason = Index dieser Klausel
            assign(other, trail.currentLevel(), static_cast<int>(cidx));
            ++i;
        }
    }
    return nullptr;
}

Clause *Solver::propagate() {
    // Verarbeite alle neuen Trail-Einträge ab qhead
    const auto& tr = trail.getTrail();
    while (qhead < tr.size()) {
        Literal p = tr[qhead].lit; // nächstes unpropagiertes Literal
        ++qhead;

        // p == true -> ¬p ist falsifiziert: nur diese Watch-Liste bearbeiten
        Clause* confl = propagateLiteralFalse(negate(p));
        if (confl) return confl;
    }
    return nullptr;
}

void Solver::seedRootUnits() {
    for (size_t i = 0; i < clauses.size(); ++i) {
        Clause& c = clauses[i];
        if (c.size() == 1){
            const Literal& u = c.at(0);
            int var = u.getVar();
            if (assignment[var] == -1) {
                assign(u, 0, int(i));
            }
        }
    }
}