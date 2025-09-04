/*
  Trail.h
  -------
  Repräsentiert den „Trail“ (Entscheidungspfad) des Solvers.
  Hier werden alle Zuweisungen von Literalen mit ihrem Entscheidungslevel
  und ggf. der verantwortlichen Klausel gespeichert.

  Eine Zuweisung (TrailEntry) enthält:
    - das Literal (Variable + Negation),
    - den Entscheidunglevel (root = 0),
    - den Index der Reason-Klausel im Solver (oder -1, falls es eine Entscheidung war).
*/

#ifndef TRAIL_H
#define TRAIL_H

#include <vector>
#include <ostream>
#include "Literal.h"
#include "Clause.h"

class Trail {
private:
    // Ein Eintrag im Trail: Literal + Level + Reason
    struct TrailEntry {
        Literal lit;       // zugewiesenes Literal
        int     level;     // Entscheidungsebene
        int     reason_idx; // Index der Reason-Klausel im Solver::clauses; -1 = direkte Entscheidung
    };

    // Alle bisherigen Zuweisungen in zeitlicher Reihenfolge
    std::vector<TrailEntry> trail;

public:
    // Neues Literal zuweisen und im Trail speichern
    void assign(const Literal& lit, int level, int reason_idx);

    // Prüfen, ob eine Variable schon belegt wurde
    bool isAssigned(int var) const;

    // Aktuelles Entscheidungslevel (Level des letzten Eintrags oder 0)
    int  currentLevel() const;

    // Zugriff auf das zuletzt gesetzte Literal
    Literal& getLastLiteral();

    // Level einer bestimmten Variable zurückgeben (0, falls nicht zugewiesen)
    int  getLevelOfVar(int var) const;

    // Index der Klausel, die für die Zuweisung verantwortlich war
    // -1, falls Entscheidung oder nicht vorhanden
    int  getReasonIndexOfVar(int var) const;

    // Entfernt alle Einträge oberhalb eines Levels (Backtracking)
    // und gibt die entfernten Variablen zurück
    std::vector<int> popAboveLevel(int level);

    // Read-only Zugriff auf den kompletten Trail (zum Debuggen oder Iterieren)
    const std::vector<TrailEntry>& getTrail() const;

    // Ausgabeoperator für den Trail (schöne Darstellung)
    friend std::ostream& operator<<(std::ostream& os, const Trail& t);
};

#endif // TRAIL_H