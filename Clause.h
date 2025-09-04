/*
  Clause.h
  --------
  Repräsentiert eine Klausel (Disjunktion von Literalen) im CDCL-Solver.
  Diese Klasse verwaltet:
    - die enthaltenen Literale,
    - die zwei beobachteten Literale (Two-Watched-Literals),
    - LBD-Wert (Literal Block Distance) zur Qualitätsbewertung,
    - eine Aktivität (für Deletion-Policy),
    - das Flag, ob es sich um eine gelernte Klausel handelt.
*/

#ifndef CLAUSE_H
#define CLAUSE_H

#include <utility>
#include <vector>
#include <iostream>
#include "Literal.h"

// Forward-Deklaration, damit computeLBD(const Trail&) deklariert werden kann.
class Trail;

class Clause {
private:
    // Die Literale der Klausel (OR-Verknüpfung).
    std::vector<Literal> clause;

    // Indizes der zwei beobachteten Literale im Vektor 'clause'.
    // -1 bedeutet: (noch) nicht gesetzt. Bei Unit-Klauseln zeigen beide auf dasselbe Literal.
    int w0 = -1;
    int w1 = -1;

    // LBD-Wert (kleiner = besser). -1 bedeutet "noch nicht berechnet".
    int lbd = -1;

    // Aktivität der Klausel (für Deletion-Strategien). Höher = wichtiger.
    double activity = 0.0;

    // true, wenn diese Klausel gelernt wurde (nicht aus der Eingabe stammt).
    bool learnt = false;

public:
    // Konstruktor: nimmt die Literale der Klausel entgegen (optional leer).
    explicit Clause(std::vector<Literal> cla = {});

    // Initialisiert die Watch-Indizes.
    // Regel:
    //  - leere Klausel: beide -1,
    //  - Unit-Klausel: w0 = w1 = 0,
    //  - sonst: w0 = 0, w1 = 1
    void initWatchesDefault();

    // Zugriff auf die Watch-Indizes
    inline int watch0() const { return w0; }
    inline int watch1() const { return w1; }
    inline void setWatch0(int idx) { w0 = idx; }
    inline void setWatch1(int idx) { w1 = idx; }

    // Gibt zu einem Watch-Index den jeweils anderen zurück.
    inline int otherWatchIndex(int watchedIdx) const { return (watchedIdx == w0) ? w1 : w0; }

    // Bequeme Zugriffsfunktionen auf die Klausel
    inline size_t size() const { return clause.size(); }
    inline const Literal& at(size_t i) const { return clause[i]; }

    // Read-Only Zugriff auf die Literale
    const std::vector<Literal>& getClause() const;

    // Ausgabe als {l1, l2, ...}
    friend std::ostream& operator<<(std::ostream& os, const Clause& clause);

    // LBD setzen/lesen
    void setLBD(int lbdVal);
    int getLBD() const;

    // LBD berechnen (Anzahl unterschiedlicher Entscheidungsebenen in der Klausel).
    int computeLBD(const Trail& trail) const;

    // Aktivität lesen/ändern (für Deletion-Policy)
    double getActivity() const {
        return activity;
    }
    void bumpActivity(double inc) {
        activity += inc;
    }
    void decayActivity(double decay) {
        activity *= decay;
    }

    // Gelernt-Flag lesen/setzen
    bool isLearnt() const {
        return learnt;
    }
    void setLearnt(bool v) {
        learnt = v;
    }
};

#endif // CLAUSE_H