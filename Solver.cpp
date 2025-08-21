#include "Solver.h"
#include "Timer.h"

#include <algorithm>
#include <iostream>
#include <cassert>
#include <unordered_set>

// i >= 1
int luby(int i) {
    assert(i >= 1);
    int k = 1;
    // Finde k mit 2^k - 1 >= i
    while (((1 << k) - 1) < i) ++k;

    if (((1 << k) - 1) == i) {
        return 1 << (k - 1);
    }
    // Rekursiver Fall
    return luby(i - (1 << (k -1)) + 1);
}

Solver::Solver(int n) : numVars(n), assignment(numVars + 1, -1), savedPhase(numVars + 1, -1) {
    heuristic.initialize(numVars);
    //currentHeuristic = HeuristicType::RANDOM;
    currentHeuristic = HeuristicType::JEROSLOW_WANG; // JW-TS
    watchList.assign(2 * numVars, {}); // 2 Einträge je Variable (pos/neg)

    restart_budget = restart_base * luby(restart_idx);
    conflicts_since_restart = 0;
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

            // LBD berechnen, in Klausel schreiben und Stats updaten
            const int lbd = learnedClause.computeLBD(trail);
            learnedClause.setLBD(lbd);
            stats.learnt_lbd_sum   += static_cast<uint64_t>(lbd);
            stats.learnt_lbd_count += 1;
            if (lbd <= 2)      stats.learnt_lbd_le2 += 1;
            else if (lbd <= 4) stats.learnt_lbd_3_4 += 1;
            else               stats.learnt_lbd_ge5 += 1;

            // Reorder: assertierendes Literal an Index 0
            {
                std::vector<Literal> tmp;
                tmp.reserve(learnedClause.size());
                tmp.push_back(assertLit);
                for (const auto& l : learnedClause.getClause()) {
                    if (!(l.getVar() == assertLit.getVar() && l.isNegated() == assertLit.isNegated()))
                        tmp.push_back(l);
                }
                learnedClause = Clause(std::move(tmp));
            }

            addClause(learnedClause);
            stats.learnts_added++;
            if ((stats.learnts_added % 200) == 0) {
                reduceDB();
            }
            int reason_idx = static_cast<int>(clauses.size()) - 1;

            assign(assertLit, backjumpLevel, reason_idx);
            conflicts_since_restart++;
            if (conflicts_since_restart >= restart_budget) {
                backtrackToLevel(0);
                restart_idx++;
                restart_budget = restart_base * luby(restart_idx);
                conflicts_since_restart = 0;
                stats.restarts++;
                continue;
            }
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
    // Stats: decision vs propagation
    if (reason_idx == -1) {
        stats.decisions++;
    } else {
        stats.propagations++;
    }
    trail.assign(lit, level, reason_idx); // an Trail anhängen (enqueue)
    assignment[lit.getVar()] = lit.isNegated() ? 0 : 1;
    savedPhase[lit.getVar()] = lit.isNegated() ? 0 : 1;
}

std::tuple<Clause,int,Literal> Solver::analyzeConflict(const Clause* conflict) {
    ScopedTimer _t(stats.t_analyze_ms);
    decayClauseInc(); // one decay per conflict
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
        // bump activity of the reason clause used for resolution
        if (reason) bumpClauseActivity(*const_cast<Clause*>(reason));

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

    // bump activity of the learned clause itself
    bumpClauseActivity(learnedClause);
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
    int  var        = -1;
    bool useNegated = false;
    bool jwNegHint  = false; // nur als Fallback, falls noch keine Phase gespeichert

    switch (currentHeuristic) {
        case HeuristicType::RANDOM: {
            heuristic.update(trail, numVars);
            var = heuristic.pickRandomVar();
            break;
        }
        case HeuristicType::JEROSLOW_WANG: {
            auto [v, neg] = heuristic.pickJeroslowWangVar(assignment);
            var = v;
            jwNegHint = neg;      // nur nutzen, wenn savedPhase[var] noch unbekannt ist
            break;
        }
        default: {
            break;
        }
    }

    // Guard/Fallback: erste unbelegte Variable suchen
    if (var <= 0 || var > numVars || assignment[var] != -1) {
        var = -1;
        for (int v = 1; v <= numVars; ++v) {
            if (assignment[v] == -1) { var = v; break; }
        }
        if (var == -1) var = 1;
    }

    // Polarity-Regel:
    // - Wenn Phase-Saving schon eine Phase kennt, nutze die (fair & stabil).
    // - Sonst (nur beim ersten Mal) nimm die JW-Empfehlung.
    if (savedPhase[var] != -1) {
        useNegated = (savedPhase[var] == 0);   // 0 == zuletzt NEGATIV
    } else {
        useNegated = jwNegHint;
    }

    return Literal{var, useNegated};
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

    if (c.getLBD() < 0) {
        c.setLBD(c.computeLBD(trail));
    }

    if (currentHeuristic == HeuristicType::JEROSLOW_WANG) {
        heuristic.updateJeroslowWang(c);
    }
}

void Solver::printModel() const {
    for (int i = 1; i <= numVars; ++i) {
        std::cout << "x" << i << " = "
        << (assignment[i] == -1 ? "Unassigned"
            : (assignment[i] == 1 ? "True" : "False")) << "\n";
    }
}

void Solver::printStats() const {
    std::cout << "Stats:\n"
        << "decisions=" << stats.decisions
        << " conflicts=" << stats.conflicts
        << " propagations=" << stats.propagations
        << " learnts_added=" << stats.learnts_added
        << " inspections=" << stats.clause_inspections
        << " watch_moves=" << stats.watch_moves
        << " t_bcp_ms=" << stats.t_bcp_ms
        << " restarts=" << stats.restarts
        << " lbd_avg=" << (stats.learnt_lbd_count ? (double)stats.learnt_lbd_sum / (double)stats.learnt_lbd_count : 0.0)
        << " lbd_le2=" << stats.learnt_lbd_le2
        << " lbd_3_4=" << stats.learnt_lbd_3_4
        << " lbd_ge5=" << stats.learnt_lbd_ge5
        << " deleted=" << stats.deleted_count
        << " deleted_lbd_sum=" << stats.deleted_lbd_sum
        << "\n";
}


Clause* Solver::propagateLiteralFalse(const Literal &falsified) {
    // Nur Klauseln betrachten, die das aktuell falsifizierte Literal beobachten
    auto& wl = watchList[litToIndex(falsified)];
    size_t i = 0;
    while (i < wl.size()) {
        size_t cidx = wl[i];
        Clause* C = &clauses[cidx];
        stats.clause_inspections++;

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
                stats.watch_moves++;
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

    ScopedTimer _t(stats.t_bcp_ms);

    // Verarbeite alle neuen Trail-Einträge ab qhead
    const auto& tr = trail.getTrail();
    while (qhead < tr.size()) {
        Literal p = tr[qhead].lit; // nächstes unpropagiertes Literal
        ++qhead;

        // p == true -> ¬p ist falsifiziert: nur diese Watch-Liste bearbeiten
        Clause* confl = propagateLiteralFalse(negate(p));
        if (confl) {
            stats.conflicts++;
            return confl;
        }
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

void Solver::reduceDB() {
    // --- Kandidaten sammeln: keine Units/Binaries, LBD>2, nicht locked ---
    struct Candidate { size_t idx; int lbd; size_t sz; double act; };
    std::vector<Candidate> cand;
    cand.reserve(clauses.size());

    auto isLocked = [&](size_t cidx) -> bool {
        for (const auto& e : trail.getTrail()) {
            if (e.reason_idx == static_cast<int>(cidx)) return true;
        }
        return false;
    };

    for (size_t i = 0; i < clauses.size(); ++i) {
        Clause& c = clauses[i];
        const size_t sz = c.size();
        if (sz <= 2) continue;              // Units/Binaries behalten
        const int lbd = c.getLBD();
        if (lbd <= 2) continue;             // sehr gute Klauseln behalten
        if (isLocked(i)) continue;          // Reason-Klauseln behalten
        cand.push_back({i, lbd, sz, clauses[i].getActivity()});
    }

    if (cand.size() < 2) return;

    // Schlechteste zuerst: hohe LBD, dann niedrige Aktivität, dann längere Klausel
    std::sort(cand.begin(), cand.end(), [](const Candidate& a, const Candidate& b){
        if (a.lbd != b.lbd) return a.lbd > b.lbd;   // higher LBD is worse
        if (a.act != b.act) return a.act < b.act;   // lower activity is worse
        return a.sz  > b.sz;                        // larger clauses are worse
    });


    // Ungefähr die Hälfte der Kandidaten löschen
    size_t toRemove = cand.size() / 2;
    if (toRemove == 0) return;

    // LBD-Stats für die zu löschenden Kandidaten sammeln
    for (size_t k = 0; k < toRemove; ++k) {
        stats.deleted_count   += 1;
        stats.deleted_lbd_sum += static_cast<uint64_t>(cand[k].lbd);
    }

    // Markiere zu löschende Klauseln
    std::vector<bool> mark(clauses.size(), false);
    for (size_t k = 0; k < toRemove; ++k) {
        mark[cand[k].idx] = true;
    }

    // Watches der Klausel abmelden
    auto detachBothWatches = [&](size_t cidx) {
        Clause& C = clauses[cidx];
        int w0 = C.watch0();
        int w1 = C.watch1();
        if (w0 != -1) detachClause(cidx, C.at(w0));
        if (w1 != -1) detachClause(cidx, C.at(w1));
    };

    // Nach Erase: alle Watch-Listen-Indizes > removedIdx dekrementieren
    auto adjustWatchListsAfterErase = [&](size_t removedIdx) {
        for (auto& wl : watchList) {
            for (auto& idx : wl) {
                if (idx > removedIdx) --idx;
            }
        }
    };

    // Rückwärts löschen (stabile Indizes) + Watchlisten anpassen
    size_t i = clauses.size();
    while (i > 0) {
        --i; // dekrementiere zuerst
        if (!mark[i]) continue;
        detachBothWatches(i);
        clauses.erase(clauses.begin() + i);
        adjustWatchListsAfterErase(i);
        // Trail-Reason-Anpassungen nicht nötig: locked-Klauseln wurden ausgeschlossen
    }
}

void Solver::setHeuristicSeed(uint64_t s) {
    heuristic.setSeed(s);
}

void Solver::bumpClauseActivity(Clause &c) {
    c.bumpActivity(clauseInc);
    if (c.getActivity() > 1e100) { // Re-scale
        for (auto& cl : clauses) {
            cl.decayActivity(1e-100);
        }
        clauseInc *= 1e-100;
    }
}

void Solver::decayClauseInc() {
    clauseInc /= clauseDecay;
}
