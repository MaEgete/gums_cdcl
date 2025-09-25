#include "Solver.h"
#include "Timer.h"

#include <algorithm>
#include <iostream>
#include <cassert>
#include <unordered_set>
#include <iomanip>
#include <fstream>

// Luby-Folge (i >= 1):
// Liefert das i-te Element der Luby-Sequenz (für Restart-Budgets).
int luby(int i) {
    int k = 1;
    while (true) {
        int two_k     = static_cast<int>(std::pow(2, k));
        int bound     = two_k - 1;
        int prevBound = static_cast<int>(std::pow(2, k - 1));

        if (i == bound) {
            return prevBound;
        }
        if (i >= prevBound && i < bound) {
            return luby(i - prevBound + 1);
        }
        k++;
    }
}

// Konstruktor: Größe setzen, Grundstrukturen vorbereiten
Solver::Solver(int n)
        : numVars(n),
          assignment(numVars + 1, -1),   // -1 = unbelegt; Index 0 bleibt ungenutzt
          savedPhase(numVars + 1, -1)    // -1 = keine gespeicherte Phase
{
    // Random-Grundinitialisierung (für Random-Heuristik)
    heuristic.initialize(numVars);
    // VSIDS-Strukturen vorbereiten (Heap/Activity)
    heuristic.initializeVSIDS(numVars, 0.95);

    // currentHeuristic wird extern gesetzt (setHeuristic)
    // currentHeuristic = HeuristicType::RANDOM;
    // currentHeuristic = HeuristicType::JEROSLOW_WANG; // JW-TS

    // Für jedes Literal eine Watch-Liste: pos/neg → 2 pro Variable
    watchList.assign(2 * numVars, {});

    // Restart-Budget initialisieren (Luby)
    restart_budget = restart_base * luby(restart_idx);
    conflicts_since_restart = 0;
}

// Hauptschleife des Solvers
bool Solver::solve() {
    // Optional Puffer für gelernte Klauseln (reduziert Reallocs)
    if (!clauses.empty()) clauses.reserve(clauses.size() + 1024);

    // Bestehende Klauseln an Watch-Listen hängen
    attachExistingClauses();
    // Unit-Klauseln (Level 0) vorab in den Trail
    seedRootUnits();

    // JW-Scores einmalig aus allen Klauseln berechnen (nur wenn JW aktiv)
    if (currentHeuristic == HeuristicType::JEROSLOW_WANG) {
        heuristic.initializeJeroslowWang(clauses, numVars);
    }

    // CDCL-Schleife
    int i = 0;
    while (true) {
        std::cout << "Conflicts: " << stats.conflicts << std::endl;
        std::cout << "i: " << i++ << std::endl;
        // BCP (Two-Watched-Literals)
        Clause* conflict = propagate();
        if (conflict != nullptr) {
            std::cout << "KONFLIKT" << std::endl;
            std::cout << "Level: " << decisionLevel << std::endl;

            // Konflikt auf Root-Level → UNSAT
            if (decisionLevel == 0) return false;

            // 1-UIP Analyse → (gelernte Klausel, Backjump-Level, assertierendes Literal)
            auto [learnedClause, backjumpLevel, assertLit] = analyzeConflict(conflict);
            std::cout << "KONFLIKT2" << std::endl;

            // Backjump
            backtrackToLevel(backjumpLevel);

            // LBD der gelernten Klausel bestimmen + Stats aktualisieren
            const int lbd = learnedClause.computeLBD(trail);
            learnedClause.setLBD(lbd);
            stats.learnt_lbd_sum += static_cast<uint64_t>(lbd);
            stats.learnt_lbd_count++;
            if (lbd <= 2)      stats.learnt_lbd_le2++;
            else if (lbd <= 4) stats.learnt_lbd_3_4++;
            else               stats.learnt_lbd_ge5++;

            // Reorder: assertierendes Literal an Position 0 (üblich für BCP)
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

            // Gelernte Klausel hinzufügen (inkl. Watches) + ggf. Datenbank reduzieren
            addClause(learnedClause);
            stats.learnts_added++;
            if ((stats.learnts_added % 200) == 0) {
                reduceDB();
            }
            int reason_idx = static_cast<int>(clauses.size()) - 1;

            // Assertierendes Literal direkt setzen (am Backjump-Level)
            assign(assertLit, backjumpLevel, reason_idx);
            conflicts_since_restart++;

            // Seltene Statusausgabe (alle 1000 Konflikte)
            if ((stats.conflicts % 10) == 0) {
                double lbd_avg = stats.learnt_lbd_count
                    ? (double)stats.learnt_lbd_sum / (double)stats.learnt_lbd_count
                    : 0.0;
                std::cout << "[conf=" << stats.conflicts
                          << " restarts=" << stats.restarts
                          << " learnts=" << stats.learnts_added
                          << " lbd_avg=" << lbd_avg
                          << " qhead=" << qhead
                          << " dl=" << decisionLevel
                          << "]\n";
            }

            // Restart nach Budget (Luby)
            if (conflicts_since_restart >= restart_budget) {
                backtrackToLevel(0);
                restart_idx++;
                restart_budget = restart_base * luby(restart_idx);
                conflicts_since_restart = 0;
                stats.restarts++;
                continue;
            }
            continue; // nach Konflikt weiter propagieren
        }
        else if (allVariablesAssigned()) {
            // Keine Konflikte und alles belegt → SAT
            return true;
        }
        else {
            // Branching-Entscheidung treffen
            Literal decision = pickBranchingVariable();
            decisionLevel++;
            assign(decision, decisionLevel, -1); // -1 = Entscheidung (keine Reason-Klausel)
        }
    }
}

// Literal (pos/neg) auf Index in watchList abbilden:
// pos(x) -> (var-1)*2, neg(x) -> (var-1)*2 + 1
int Solver::litToIndex(const Literal &l) const {
    return (l.getVar() - 1) * 2 + (l.isNegated() ? 1 : 0);
}

// Negation eines Literals bauen
Literal Solver::negate(const Literal &l) const {
    return Literal(l.getVar(), !l.isNegated());
}

// Klausel an die Watch-Liste des beobachteten Literals hängen
void Solver::attachClause(size_t clauseIdx, const Literal &w) {
    watchList[litToIndex(w)].push_back(clauseIdx);
}

// Klausel aus Watch-Liste lösen (swap-remove)
void Solver::detachClause(size_t clauseIdx, const Literal &w) {
    auto& v = watchList[litToIndex(w)];
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == clauseIdx) {
            v[i] = v.back();
            v.pop_back();
            return;
        }
    }
}

// Alte (langsame) Propagation: nur Vergleichszwecke
Clause* Solver::unitPropagation() {
    bool propagated;
    do {
        propagated = false;
        for (size_t i = 0; i < clauses.size(); ++i) {
            auto& cla = clauses[i];

            bool    satisfied     = false;
            int     unsetLits     = 0;
            Literal unassignedLit(0,false);

            // Klausel prüfen: erfüllt? Unit? Konflikt?
            for (const auto& lit : cla.getClause()) {
                int var = lit.getVar();
                int val = assignment[var];

                // Literal ist wahr?
                if ((val == 1 && !lit.isNegated()) || (val == 0 && lit.isNegated())) {
                    satisfied = true;
                    break;
                }
                // Unbelegt → potentieller Unit-Kandidat
                if (val == -1) {
                    ++unsetLits;
                    unassignedLit = lit;
                }
            }

            if (satisfied) continue;
            if (unsetLits == 1) {
                // Unit → zuweisen
                assign(unassignedLit, decisionLevel, static_cast<int>(i));
                propagated = true;
            }
            if (unsetLits == 0) {
                // alle falsch → Konflikt
                return &cla;
            }
        }
    } while (propagated);
    return nullptr;
}

// Literal in den Trail schreiben, Stats pflegen, Phase speichern
void Solver::assign(const Literal& lit, int level, int reason_idx) {
    // Entscheidung vs. Propagation
    if (reason_idx == -1) {
        stats.decisions++;
    } else {
        stats.propagations++;
    }
    // In den Trail (enqueue)
    trail.assign(lit, level, reason_idx);
    // Belegung setzen
    assignment[lit.getVar()] = lit.isNegated() ? 0 : 1;
    // Phase-Saving (0 = neg; 1 = pos)
    savedPhase[lit.getVar()] = lit.isNegated() ? 0 : 1;
}

// Konfliktanalyse (1-UIP): gelernte Klausel, Backjump-Level, assertierendes Literal
std::tuple<Clause,int,Literal> Solver::analyzeConflict(const Clause* conflict) {
    std::cout << "KONFLIKT3" << std::endl;
    ScopedTimer _t(stats.t_analyze_ms);   // Analysezeit messen
    decayClauseInc(); // pro Konflikt genau einmal das Klausel-Inkrement zerfallen lassen
    Clause learnedClause = *conflict;     // Start mit Konfliktklausel

    // Für VSIDS: gemerkte Variablen in dieser Analyse
    std::vector<uint8_t> seen_vars(numVars + 1, 0);
    std::vector<int> seen_list;
    seen_list.reserve(32);

    auto markSeen = [&](int v) {
        if (v >= 1 && v <= numVars && !seen_vars[v]) {
            seen_vars[v] = 1;
            seen_list.push_back(v);
        }
    };

    // Alle Variablen der Startklausel markieren
    for (const auto& l : learnedClause.getClause()) {
        markSeen(l.getVar());
    }

    const int currentLevel = decisionLevel;

    // Zählt, wie viele Literale der Klausel auf einem Level liegen
    auto countAtLevel = [&](int lvl) {
        int c = 0;
        for (const auto& l : learnedClause.getClause()) {
            if (trail.getLevelOfVar(l.getVar()) == lvl) ++c;
        }
        return c;
    };

    // Resolve bis 1-UIP erreicht ist (genau 1 Literal auf currentLevel bleibt)
    while (countAtLevel(currentLevel) > 1) {
        // jüngstes Literal aus currentLevel finden, das in learnedClause vorkommt
        Literal resolveLit(0,false);
        for (auto it = trail.getTrail().rbegin(); it != trail.getTrail().rend(); ++it) {
            if (trail.getLevelOfVar(it->lit.getVar()) != currentLevel) continue;
            bool inLearned = std::any_of(
                learnedClause.getClause().begin(), learnedClause.getClause().end(),
                [&](const Literal& L){ return L.getVar() == it->lit.getVar(); }
            );
            if (inLearned) { resolveLit = it->lit; break; }
        }

        int before_cnt = countAtLevel(currentLevel);

        // Reason-Klausel besorgen
        int reason_idx = trail.getReasonIndexOfVar(resolveLit.getVar());
        const Clause* reason = (reason_idx >= 0 && reason_idx < static_cast<int>(clauses.size()))
                             ? &clauses[reason_idx] : nullptr;
        if (!reason) break; // defensiv

        // Reason-Klausel aktivieren (Clause-Aktivität erhöhen)
        bumpClauseActivity(*const_cast<Clause*>(reason));

        // VSIDS: Variablen aus Reason + aktueller learnedClause markieren
        for (const auto& l : reason->getClause()) {
            markSeen(l.getVar());
        }
        for (const auto& l : learnedClause.getClause()) {
            markSeen(l.getVar());
        }

        // Auflösung über resolveLit
        std::vector<Literal> newLits;
        auto litEquals = [](const Literal& a, const Literal& b){
            return a.getVar() == b.getVar() && a.isNegated() == b.isNegated();
        };

        // learnedClause: GLEICHES Vorzeichen wie resolveLit entfernen (resolveLit selbst)
        for (const auto& l : learnedClause.getClause()) {
            if (l.getVar() == resolveLit.getVar() && l.isNegated() == resolveLit.isNegated()) continue;
            if (std::ranges::none_of(newLits, [&](const Literal& x){ return litEquals(x,l); }))
                newLits.push_back(l);
        }
        // reason: KOMPLEMENT zu resolveLit entfernen
        for (const auto& l : reason->getClause()) {
            if (l.getVar() == resolveLit.getVar() && l.isNegated() != resolveLit.isNegated()) continue;
            if (std::ranges::none_of(newLits, [&](const Literal& x){ return litEquals(x,l); }))
                newLits.push_back(l);
        }

        learnedClause = Clause(std::move(newLits));
        int after_cnt = countAtLevel(currentLevel);
        std::cout << "[analyze] before=" << before_cnt << " after=" << after_cnt
                  << " | resolveLit=" << (resolveLit.isNegated()?"-":"+") << resolveLit.getVar() << std::endl;
        if (after_cnt > before_cnt) {
            std::cerr << "[analyze] ERROR: clause grew on current level — breaking to avoid infinite loop" << std::endl;
            break;
        }
    }

    // Gelernte Klausel selbst ebenfalls aktivieren (Clause-Activity)
    bumpClauseActivity(learnedClause);

    // Als gelernt markieren (für Deletion-Policy)
    learnedClause.setLearnt(true);

    // assertierendes Literal = einziges Literal auf currentLevel
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

    // VSIDS: alle gesehenen Variablen bumpen; danach globales Decay des varInc
    if (currentHeuristic == HeuristicType::VSIDS) {
        for (int v : seen_list) {
            heuristic.vsidsBump(v);
        }
        heuristic.vsidsDecayInc();
    }

    return {learnedClause, backjumpLevel, assertLit};
}

// Backtrack/Backjump auf gegebenes Level
void Solver::backtrackToLevel(int level) {
    // Einträge oberhalb des Levels aus dem Trail entfernen
    auto popped = trail.popAboveLevel(level);
    for (int var : popped) {
        assignment[var] = -1; // wieder unbelegt
        // VSIDS: Variable wieder in den Heap aufnehmen
        if (currentHeuristic == HeuristicType::VSIDS) {
            heuristic.onBacktrackUnassign(var);
        }
    }

    decisionLevel = level;

    // qhead darf nie größer als die aktuelle Trail-Länge sein
    if (qhead > trail.getTrail().size()) {
        qhead = trail.getTrail().size();
    }
}

// Watch-Listen für bereits existierende Klauseln aufbauen
void Solver::attachExistingClauses() {
    for (size_t idx = 0; idx < clauses.size(); ++idx){
        auto& c = clauses[idx];
        // Default-Watches setzen, falls noch nicht initialisiert
        if (c.size() > 0 && (c.watch0() == -1 || c.watch1() == -1)) {
            c.initWatchesDefault();
        }
        // An die entsprechenden Watch-Listen hängen
        if (c.size() == 1) {
            attachClause(idx, c.at(0)); // Unit: nur einmal attachen
        } else {
            if (c.watch0() != -1) attachClause(idx, c.at(c.watch0()));
            if (c.watch1() != -1) attachClause(idx, c.at(c.watch1()));
        }
    }
}

// Branching-Variable wählen (abhängig von der Heuristik)
Literal Solver::pickBranchingVariable() {
    int  var        = -1;   // gewählte Variable
    bool useNegated = false; // gewählte Polarität (true = negiert)
    bool jwNegHint  = false; // JW-Empfehlung, nur wenn keine Phase gespeichert

    switch (currentHeuristic) {
        case HeuristicType::RANDOM: {
            // Random: unbelegte Variablen neu bestimmen und zufällig wählen
            heuristic.update(trail, numVars);
            var = heuristic.pickRandomVar();
            break;
        }
        case HeuristicType::JEROSLOW_WANG: {
            // JW: beste Variable + empfohlene Polarität
            auto [v, neg] = heuristic.pickJeroslowWangVar(assignment);
            var = v;
            jwNegHint = neg;      // nur nutzen, wenn savedPhase[var] noch unbekannt ist
            break;
        }
        case HeuristicType::VSIDS: {
            // VSIDS: Spitze des Heaps suchen, aber belegte Variablen überspringen
            int candidate = -1;
            while (true) {
                int top = heuristic.heapTop();
                if (top == -1) { // leerer Heap – sollte selten sein
                    candidate = -1;
                    break;
                }
                if (assignment[top] != -1) {
                    // Belegte Spitze verwerfen und weitersuchen
                    heuristic.heapPop();
                    continue;
                }
                // Unbelegte Spitze gefunden – NICHT poppen (entspricht altem Verhalten)
                candidate = top;
                break;
            }
            var = candidate;
            break;
        }
        default: {
            break;
        }
    }

    // Sicherheits-Fallback: erste unbelegte Variable nehmen
    if (var <= 0 || var > numVars || assignment[var] != -1) {
        var = -1;
        for (int v = 1; v <= numVars; ++v) {
            if (assignment[v] == -1) { var = v; break; }
        }
        if (var == -1) var = 1;
    }

    // Polarität:
    // - Wenn Phase-Saving etwas kennt, nutze das (stabil, fair).
    // - Sonst (nur beim ersten Mal) die JW-Empfehlung übernehmen.
    if (savedPhase[var] != -1) {
        useNegated = (savedPhase[var] == 0);   // 0 == zuletzt NEGATIV
    } else {
        useNegated = jwNegHint;
    }

    return Literal{var, useNegated};
}

// Heuristik umschalten
void Solver::setHeuristic(HeuristicType type) {
    currentHeuristic = type;
}

// Prüfen, ob alle Variablen belegt sind
bool Solver::allVariablesAssigned() const {
    for (int i = 1; i <= numVars; ++i)
        if (assignment[i] == -1) return false;
    return true;
}

// Klausel hinzufügen (inkl. Watches) + ggf. JW-Update + LBD nachtragen
void Solver::addClause(const Clause& clause) {
    clauses.push_back(clause);
    size_t idx = clauses.size() - 1;
    Clause& c = clauses[idx];

    // Watches setzen, wenn nötig
    if (c.size() > 0 && (c.watch0() == -1 || c.watch1() == -1))
        c.initWatchesDefault();

    // An die Watch-Listen hängen
    if (c.size() == 1) {
        attachClause(idx, c.at(0));
    } else {
        if (c.watch0() != -1) attachClause(idx, c.at(c.watch0()));
        if (c.watch1() != -1) attachClause(idx, c.at(c.watch1()));
    }

    // LBD nachtragen, falls noch nicht gesetzt (z. B. bei Input-Klauseln)
    if (c.getLBD() < 0) {
        c.setLBD(c.computeLBD(trail));
    }

    // JW-Scores inkrementell aktualisieren (nur wenn JW aktiv)
    if (currentHeuristic == HeuristicType::JEROSLOW_WANG) {
        heuristic.updateJeroslowWang(c);
    }
}

// Modell ausgeben (Debug/Info)
void Solver::printModel() const {
    for (int i = 1; i <= numVars; ++i) {
        std::cout << "x" << i << " = "
        << (assignment[i] == -1 ? "Unassigned"
            : (assignment[i] == 1 ? "True" : "False")) << "\n";
    }
}

// Statistiken ausgeben
void Solver::printStats() const {
    std::cout << "\n========== Solver Statistics ==========\n";
    std::cout << std::left << std::setw(20) << "Decisions:"       << stats.decisions << "\n";
    std::cout << std::left << std::setw(20) << "Conflicts:"       << stats.conflicts << "\n";
    std::cout << std::left << std::setw(20) << "Propagations:"    << stats.propagations << "\n";
    std::cout << std::left << std::setw(20) << "Learnts added:"   << stats.learnts_added << "\n";
    std::cout << std::left << std::setw(20) << "Inspections:"     << stats.clause_inspections << "\n";
    std::cout << std::left << std::setw(20) << "Watch moves:"     << stats.watch_moves << "\n";
    std::cout << std::left << std::setw(20) << "BCP time (ms):"   << stats.t_bcp_ms << "\n";
    std::cout << std::left << std::setw(20) << "Analyze time (ms):" << stats.t_analyze_ms << "\n";
    std::cout << std::left << std::setw(20) << "Restarts:"        << stats.restarts << "\n";
    std::cout << std::left << std::setw(20) << "LBD avg:"         << (stats.learnt_lbd_count ? (double)stats.learnt_lbd_sum / (double)stats.learnt_lbd_count : 0.0) << "\n";
    std::cout << std::left << std::setw(20) << "LBD <= 2:"        << stats.learnt_lbd_le2 << "\n";
    std::cout << std::left << std::setw(20) << "LBD 3-4:"         << stats.learnt_lbd_3_4 << "\n";
    std::cout << std::left << std::setw(20) << "LBD >= 5:"        << stats.learnt_lbd_ge5 << "\n";
    std::cout << std::left << std::setw(20) << "Deleted clauses:" << stats.deleted_count << "\n";
    std::cout << std::left << std::setw(20) << "Deleted LBD sum:" << stats.deleted_lbd_sum << "\n";
    std::cout << std::left << std::setw(20) << "Heuristic:"       << heuristicToString(currentHeuristic) << "\n";
    std::cout << "=======================================\n\n";
}

void Solver::exportStats(const std::string& filename) const {
    std::ofstream csv_file{filename, std::ios::out | std::ios::app};

    if (!csv_file.is_open()) {
        std::cerr << "Datei \"" << filename << "\" konnte nicht geöffnet werden!";
        return;
    }

    csv_file << "Decisions;Conflicts;Propagations;Learnts_added;Inspections;Watch_moves;BCP_time_(ms);Analzye_time_(ms);Restarts;LBD_avg;LBD_<=_2;LBD_3-4;LBD_>=_5;Deleted_clauses;Deleted_LBD_sum;Heuristic\n";

    csv_file << stats.decisions << ";" << stats.conflicts << ";" << stats.propagations << ";"
            << stats.learnts_added << ";" << stats.clause_inspections << ";" << stats.watch_moves << ";"
            << stats.t_bcp_ms << ";" << stats.t_analyze_ms << ";" << stats.restarts << ";"
            << (stats.learnt_lbd_count ? (double)stats.learnt_lbd_sum / (double)stats.learnt_lbd_count : 0.0) << ";"
            << stats.learnt_lbd_le2 << ";" << stats.learnt_lbd_3_4 << ";" << stats.learnt_lbd_ge5 << ";"
            << stats.deleted_count << ";" << stats.deleted_lbd_sum << ";" << heuristicToString(currentHeuristic) << "\n";


    csv_file.close();
}


// Heuristik-Namen für die Statistik
std::string Solver::heuristicToString(HeuristicType type) {
    switch (type) {
        case HeuristicType::JEROSLOW_WANG:
            return "Jeroslow_Wang";
            break;
        case HeuristicType::RANDOM:
            return "Random";
            break;
        case HeuristicType::VSIDS:
            return "VSIDS";
            break;
    }
    return "None";
}

// Watch-Verarbeitung für ein falsifiziertes Literal:
// versuche Watch umzuhängen; sonst Unit/Conflict
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

        // Sicherheitsabfrage (sollte selten passieren)
        if (wIdx == -1) { ++i; continue; }

        // Index des anderen Watches
        const int otherIdx = (wIdx == 0 ? w1 : w0);

        const Literal& other = C->at(otherIdx);

        // Wenn der andere Watch bereits wahr ist → Klausel erfüllt
        if (assignment[other.getVar()] != -1) {
            const bool val = (assignment[other.getVar()] == 1);
            if (val != other.isNegated()) { ++i; continue; } // Literal ist wahr
        }

        // Versuche, Watch auf ein anderes nicht-falsches Literal zu verschieben
        bool moved = false;
        for (size_t k = 0; k < C->size(); ++k) {
            if ((int)k == w0 || (int)k == w1) continue; // andere Literale als die beiden Watches
            const Literal& cand = C->at(k);

            // cand ist nicht-falsch? (unassigned oder wahr)
            int a = assignment[cand.getVar()];
            bool candFalse = (a != -1) && ( (a==1) == cand.isNegated() );

            if (!candFalse) {
                // Watch von "falsified" → "cand" umhängen
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
            // Achtung: wl[i] wurde durch swap-remove ersetzt → nicht i++ !
            continue;
        }

        // Kein Ersatz gefunden → Unit oder Konflikt
        int a = assignment[other.getVar()];
        bool otherFalse = (a != -1) && ( (a==1) == other.isNegated() );

        if (otherFalse) {
            // Beide Watches falsifiziert → Konflikt
            return C;
        } else {
            // Unit: setze "other" mit Reason = Index dieser Klausel
            assign(other, trail.currentLevel(), static_cast<int>(cidx));
            ++i;
        }
    }
    return nullptr;
}

// Neue (schnelle) Propagation via Two-Watched-Literals
Clause *Solver::propagate() {

    ScopedTimer _t(stats.t_bcp_ms); // Zeit für BCP messen

    // Verarbeite alle neuen Trail-Einträge ab qhead
    const auto& tr = trail.getTrail();
    while (qhead < tr.size()) {
        Literal p = tr[qhead].lit; // nächstes unpropagiertes Literal
        ++qhead;

        // p == true → ¬p ist falsifiziert: nur diese Watch-Liste abarbeiten
        Clause* confl = propagateLiteralFalse(negate(p));
        if (confl) {
            stats.conflicts++;
            return confl;
        }
    }
    return nullptr;
}

// Alle Unit-Klauseln auf Level 0 in den Trail legen
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

// Klausel-Datenbank reduzieren (Glucose-Style Heuristik)
void Solver::reduceDB() {
    // --- Kandidaten sammeln: keine Units/Binaries, LBD>2, nicht locked ---
    struct Candidate { size_t idx; int lbd; size_t sz; double act; };
    std::vector<Candidate> cand;
    cand.reserve(clauses.size());

    // "locked" = dient aktuell als Reason auf dem Trail → behalten
    auto isLocked = [&](size_t cidx) -> bool {
        for (const auto& e : trail.getTrail()) {
            if (e.reason_idx == static_cast<int>(cidx)) return true;
        }
        return false;
    };

    // Auswahl der zu prüfenden gelernten Klauseln
    for (size_t i = 0; i < clauses.size(); ++i) {
        Clause& c = clauses[i];
        if (!c.isLearnt()) continue;        // Eingabeklauseln nie löschen
        const size_t sz = c.size();
        if (sz <= 2) continue;              // Units/Binaries behalten
        const int lbd = c.getLBD();
        if (lbd <= 2) continue;             // sehr gute Klauseln behalten
        if (isLocked(i)) continue;          // Reason-Klauseln behalten
        cand.push_back({i, lbd, sz, clauses[i].getActivity()});
    }

    if (cand.size() < 2) return;

    // Sortierung "schlechteste zuerst":
    //  1) höhere LBD
    //  2) niedrigere Aktivität
    //  3) längere Klausel
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

// Seed für Random-Heuristik weiterreichen
void Solver::setHeuristicSeed(uint64_t s) {
    heuristic.setSeed(s);
}

// Aktivität einer Klausel erhöhen + Rescale-Schutz
void Solver::bumpClauseActivity(Clause &c) {
    c.bumpActivity(clauseInc);
    if (c.getActivity() > 1e100) { // Re-scale
        for (auto& cl : clauses) {
            cl.decayActivity(1e-100);
        }
        clauseInc *= 1e-100;
    }
}

// Zerfall des Klausel-Inkrements (wie bei VSIDS varInc)
void Solver::decayClauseInc() {
    clauseInc /= clauseDecay;
}