/*
Literal.cpp
  -----------
  Implementierung der Klasse Literal.
  Ein Literal besteht aus einer Variablen-ID und einem Negations-Flag.
  Beispiele:
    - x3  (Variable 3 positiv)
    - ¬x3 (Variable 3 negiert)
*/

#include "Literal.h"

// Konstruktor: setzt die Variablen-ID und ob das Literal negiert ist
Literal::Literal(const int v, const bool neg) : var{v}, negated{neg} {}

// Gibt die Variablen-ID zurück
int  Literal::getVar() const {
    return this->var;
}

// true, wenn das Literal negiert ist
bool Literal::isNegated() const {
    return this->negated;
}

// Gibt eine Ganzzahl-Repräsentation zurück:
//   - positiv für nicht-negierte Variablen (x3 → 3)
//   - negativ für negierte Variablen (¬x3 → -3)
int Literal::toInt() const {
    return negated ? -var : var;
}

// Ausgabeoperator für Literale.
// Beispiel: "x3" oder "¬x3"
std::ostream& operator<<(std::ostream& os, const Literal& lit) {
    os << (lit.negated ? "¬" : "") << "x" << lit.var;
    return os;
}

// Zwei Literale sind gleich, wenn sowohl die Variablen-ID
// als auch das Negations-Flag übereinstimmen
bool Literal::operator==(const Literal& other) const {
    return var == other.var && negated == other.negated;
}