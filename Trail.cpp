// Trail.cpp
// ---------
// Implementierung des Trails (Entscheidungspfad). Hier werden alle
// Zuweisungen (Literal, Entscheidungslevel, Reason-Index) in zeitlicher
// Reihenfolge gespeichert. Der Trail wird u. a. zum Backtracking genutzt.

#include "Trail.h"
#include <iostream>

// Fügt eine neue Zuweisung am Ende des Trails an
// lit: gesetztes Literal; level: Entscheidungsebene; reason_idx: Index der Klausel, die zugewiesen hat (-1 = Entscheidung)
void Trail::assign(const Literal& lit, int level, int reason_idx) {
    trail.push_back({lit, level, reason_idx});
}

// Prüft, ob die Variable bereits irgendwo im Trail gesetzt wurde
bool Trail::isAssigned(int var) const {
    for (const auto& e : trail) if (e.lit.getVar() == var) return true;
    return false;
}

// Liefert das aktuelle Entscheidungslevel (0, wenn noch keine Zuweisung vorliegt)
int Trail::currentLevel() const {
    return trail.empty() ? 0 : trail.back().level;
}

// Referenz auf das zuletzt gesetzte Literal (Vorsicht: nur aufrufen, wenn der Trail nicht leer ist!)
Literal& Trail::getLastLiteral() {
    return trail.back().lit;
}

// Gibt das Entscheidungslevel einer bestimmten Variable zurück (0, wenn nicht zugewiesen)
int Trail::getLevelOfVar(int var) const {
    for (const auto& e : trail) if (e.lit.getVar() == var) return e.level;
    return 0; // unassigned → Level 0
}

// Liefert den Reason-Index für die letzte Zuweisung der Variable (oder -1 bei Entscheidung / nicht gefunden)
int Trail::getReasonIndexOfVar(int var) const {
    for (auto it = trail.rbegin(); it != trail.rend(); ++it) {
        if (it->lit.getVar() == var) return it->reason_idx;
    }
    return -1; // Variable noch nicht zugewiesen
}

// Entfernt alle Einträge mit Level > given level (Backtracking) und gibt die betroffenen Variablen zurück
std::vector<int> Trail::popAboveLevel(int level) {
    std::vector<int> popped;
    while (!trail.empty() && trail.back().level > level) {
        popped.push_back(trail.back().lit.getVar());
        trail.pop_back();
    }
    return popped;
}

// Read-only Zugriff auf den gesamten Trail (z. B. fürs Debugging oder Statistiken)
const std::vector<Trail::TrailEntry>& Trail::getTrail() const {
    return trail;
}

// Schöne Ausgabe des Trails:  { x3[L2, R:17], x7[L2, R:decision], ... }
std::ostream& operator<<(std::ostream& os, const Trail& t) {
    os << "{ ";
    for (size_t i = 0; i < t.trail.size(); ++i) {
        const auto& e = t.trail[i];
        os << e.lit << "[L" << e.level << ", R:";
        if (e.reason_idx >= 0) os << e.reason_idx; else os << "decision";
        os << "]";
        if (i + 1 < t.trail.size()) os << ", ";
    }
    os << " }";
    return os;
}