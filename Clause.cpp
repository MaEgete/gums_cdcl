// Clause.cpp
// -----------
// Implementierung der Klausel-Klasse. Eine Klausel ist eine Disjunktion von
// Literalen. Diese Datei enthält u. a. die Initialisierung der zwei Watch-Indices
// (Two-Watched-Literals) sowie die Berechnung des LBD-Werts.

#include "Clause.h"

#include <unordered_set>

#include "Trail.h" // benötigt für computeLBD (Abfrage der Entscheidungsebenen)

// Konstruktor: übernimmt die Literale. "learnt" bleibt standardmäßig false.
Clause::Clause(std::vector<Literal> cla) : clause{std::move(cla)}, learnt(false) {}

// Setzt die Standard-Watches:
//  - leere Klausel: beide auf -1 (ungültig)
//  - Unit-Klausel: beide auf 0 (zeigen auf dasselbe Literal)
//  - sonst: w0=0, w1=1 (die ersten zwei Literale beobachten)
void Clause::initWatchesDefault() {
    if (clause.empty()) { w0 = w1 = -1; return; }
    if (clause.size() == 1) { w0 = 0; w1 = 0; return; }
    w0 = 0;
    w1 = 1;
}

// Read-only Zugriff auf alle Literale der Klausel
const std::vector<Literal>& Clause::getClause() const {
    return this->clause;
}

// Schöne Ausgabe als {l1, l2, ...}
std::ostream& operator<<(std::ostream& os, const Clause& clause) {
    os << "{";
    for (size_t i = 0; i < clause.clause.size(); ++i) {
        os << clause.clause[i];
        if (i + 1 < clause.clause.size()) os << ", ";
    }
    os << "}";
    return os;
}

// LBD setzen/lesen (Literal Block Distance: Anzahl unterschiedlicher Levels)
void Clause::setLBD(int lbdVal) {
    lbd = lbdVal;
}

int Clause::getLBD() const {
    return lbd;
}

// LBD berechnen:
//  - leere Klausel: 0 (sollte praktisch nicht vorkommen)
//  - Unit: 1
//  - Binary: 1 wenn beide Literale auf gleichem Level, sonst 2
//  - sonst: Anzahl unterschiedlicher Entscheidungsebenen der enthaltenen Variablen
int Clause::computeLBD(const Trail &trail) const {
    if (clause.empty()) return 0;
    if (clause.size() == 1) return 1;

    // Spezialfall Binary-Klausel: schneller Check ohne Set
    if (clause.size() == 2) {
        int l0 = trail.getLevelOfVar(clause[0].getVar());
        int l1 = trail.getLevelOfVar(clause[1].getVar());
        return (l0 == l1) ? 1 : 2;
    }

    // Allgemeiner Fall: sammle alle Level, Größe des Sets ist der LBD
    std::unordered_set<int> levels;
    levels.reserve(clause.size());
    for (const auto& lit : clause) {
        levels.insert(trail.getLevelOfVar(lit.getVar()));
    }
    return static_cast<int>(levels.size());
}