#ifndef SOLVER_H
#define SOLVER_H

#include <vector>
#include "Clause.h"
#include "Trail.h"
#include "Heuristic.h"

//
// Solver.h
// ---------
// Deklaration des CDCL-Solvers:
//  - verwaltet Klauseln, Trail, Heuristiken und Restart/Deletion-Strategien
//  - implementiert Unit-Propagation (Two-Watched-Literals), Konfliktanalyse,
//    Backtracking/Backjumping, Restart-Luby, Clause-Deletion (Glucose-Style)
//  - stellt Statistiken bereit
//

struct Stats {
    // Zählwerte
    uint64_t decisions=0, conflicts=0, propagations=0;  // Entscheidungen, Konflikte, Propagationseinträge
    uint64_t learnts_added=0, restarts=0;               // #gelernter Klauseln, #Restarts
    uint64_t clause_inspections=0, watch_moves=0;       // #besuchte Klauseln, #Watch-Verschiebungen
    uint64_t t_bcp_ms=0, t_analyze_ms=0;                // Zeiten (ms) für BCP und Analyse

    // LBD-Statistiken (Qualität gelernter Klauseln)
    uint64_t learnt_lbd_sum   = 0;  // Summe der LBDs gelernter Klauseln
    uint64_t learnt_lbd_count = 0;  // Anzahl gelernter Klauseln (für die LBD berechnet wurde)
    uint64_t learnt_lbd_le2   = 0;  // #gelernter Klauseln mit LBD <= 2
    uint64_t learnt_lbd_3_4   = 0;  // #gelernter Klauseln mit LBD in {3,4}
    uint64_t learnt_lbd_ge5   = 0;  // #gelernter Klauseln mit LBD >= 5

    // Deletion-Statistiken
    uint64_t deleted_count    = 0;  // #gelöschter Klauseln in reduceDB
    uint64_t deleted_lbd_sum  = 0;  // Summe der LBDs gelöschter Klauseln
};

// verfügbare Variablenwahl-Heuristiken
enum class HeuristicType {
    RANDOM,         // zufällige Wahl einer unbelegten Variable
    JEROSLOW_WANG,  // Jeroslow-Wang (gewichtete Literal-Scores)
    VSIDS,          // (E)VSIDS: Aktivitäten + Heap
};

class Solver {
private:

    // Laufzeit- und Qualitätsstatistiken
    Stats stats;

    int numVars;                                               // Anzahl Variablen
    std::vector<std::vector<size_t>> watchList;                // pro Literal: Liste beobachtender Klausel-Indizes

    size_t qhead = 0; // Index in den Trail: bis wohin wurde bereits propagiert?

    // Hilfsfunktionen für Literale
    int     litToIndex(const Literal& l) const; // Literal → [0 .. 2*numVars-1] (pos/neg)
    Literal negate(const Literal& l) const;      // ¬Literal

    // Watch-Listen pflegen
    void attachClause(size_t clauseIdx, const Literal& w); // Klausel an Watch-Liste des Literals hängen
    void detachClause(size_t clauseIdx, const Literal& w); // Klausel aus Watch-Liste lösen

    // Propagation (Two-Watched-Literals): bearbeite die Watch-Liste des falsifizierten Literals
    Clause* propagateLiteralFalse(const Literal& falsified);

    // Kern-Datenstrukturen
    std::vector<Clause> clauses;  // alle (auch gelernte) Klauseln
    Trail               trail;    // Zuweisungsverlauf (Literal, Level, Reason)
    int                 decisionLevel = 0; // aktuelles Entscheidungslevel (root = 0)
    Heuristic           heuristic;        // Heuristik-Objekt (Random/JW/VSIDS)
    HeuristicType       currentHeuristic; // aktuell gewählte Heuristik

    // Belegungen: assignment[i] = -1 (unbelegt), 0 (false), 1 (true); Index 0 unbenutzt
    std::vector<int> assignment;

    // Phase Saving: -1 = unbekannt, 0 = prefer false (negated), 1 = prefer true (non-negated)
    std::vector<int> savedPhase;

    // Restart (Luby-Folge)
    int restart_idx = 1;                 // Index in der Luby-Folge
    int restart_base = 2;                // Basis-Multiplikator
    int conflicts_since_restart = 0;     // seit letztem Restart gezählte Konflikte
    int restart_budget = 0;              // Konfliktbudget bis zum nächsten Restart

    // Deletion-Policy (Glucose-Style): Aktivitätsskala der Klauseln
    double clauseInc = 1.0;   // Start-Inkrement
    double clauseDecay = 0.95;// Zerfallsfaktor für clauseInc

public:
    // Konstruktor: setzt Größe, initialisiert Heuristik/Strukturen
    explicit Solver(int n);

    // Hauptschleife des Solvers (liefert SAT/UNSAT)
    bool     solve();

    // Optionale alte Propagation O(n*m) (nur zu Vergleichszwecken)
    Clause*  unitPropagation();

    // Zuweisung eines Literals (Decision/Propagation) am aktuellen Level
    void assign(const Literal& lit, int level, int reason_idx);

    // Konfliktanalyse: liefert (gelernte Klausel, Backjump-Level, assertierendes Literal)
    std::tuple<Clause,int,Literal> analyzeConflict(const Clause* conflict);

    // Backjump/Backtrack auf ein bestimmtes Level
    void    backtrackToLevel(int level);

    // Branching-Entscheidung treffen (gemäß aktueller Heuristik)
    Literal pickBranchingVariable();

    // Heuristik wählen
    void setHeuristic(HeuristicType type);

    // Status/IO
    bool allVariablesAssigned() const; // true, wenn alle Variablen belegt sind
    void addClause(const Clause& clause); // Klausel hinzufügen (inkl. Watches setzen)
    void printModel() const;              // Belegung ausgeben
    void printStats() const;              // Statistiken ausgeben

    // Neue, effiziente Propagation (Two-Watched-Literals)
    Clause* propagate();

    // Bestehende Klauseln an Watch-Listen hängen (falls bereits im Vektor)
    void    attachExistingClauses();

    // Root-Level Units vorab enqueuen (Level 0)
    void    seedRootUnits();

    // Deletion-Policy: Schwache gelernte Klauseln entfernen
    void reduceDB();

    // Seed für Random-Heuristik setzen
    void setHeuristicSeed(uint64_t s);

    // Klauselaktivität erhöhen / Inkrement zerfallen lassen
    void bumpClauseActivity(Clause& c);
    void decayClauseInc();

    // Heuristik-Name als String (für Ausgabe)
    static std::string heuristicToString(HeuristicType type);
};

#endif // SOLVER_H