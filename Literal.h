

#ifndef LITERAL_H
#define LITERAL_H

#include <iostream>

class Literal {
private:
    int  var;      // Variablen-ID (1, 2, 3, ...)
    bool negated;  // true = negiert (¬x), false = positiv (x)

public:
    // Konstruktor: v = Variablen-ID, neg = Negations-Flag
    Literal(int v = 0, bool neg = false);

    // Gibt die Variablen-ID zurück
    int  getVar() const;

    // true, wenn Literal negiert ist
    bool isNegated() const;

    // Gibt die Ganzzahl-Repräsentation zurück:
    // positiv für nicht-negiert (x3 -> 3), negativ für negiert (¬x3 -> -3)
    int toInt() const;

    // Ausgabe als String ("x3" oder "¬x3")
    friend std::ostream& operator<<(std::ostream& os, const Literal& lit);

    // Gleichheitsoperator: zwei Literale sind gleich, wenn Var und Negation übereinstimmen
    bool operator==(const Literal& other) const;
};

#endif // LITERAL_H