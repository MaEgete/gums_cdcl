#ifndef CLAUSE_H
#define CLAUSE_H

#include <utility>
#include <vector>
#include <iostream>
#include "Literal.h"

class Trail;

class Clause {
private:
    std::vector<Literal> clause;

    // Indizes der zwei beobachteten Literale (-1 = uninitialisiert)
    int w0 = -1;
    int w1 = -1;

    int lbd = -1;

    double activity = 0.0;

public:
    explicit Clause(std::vector<Literal> cla = {});

    void initWatchesDefault();

    // Zugriff auf die Watch-Indizes
    inline int watch0() const { return w0; }
    inline int watch1() const { return w1; }
    inline void setWatch0(int idx) { w0 = idx; }
    inline void setWatch1(int idx) { w1 = idx; }

    // Den jeweils anderen Watch-Index ermitteln
    inline int otherWatchIndex(int watchedIdx) const { return (watchedIdx == w0) ? w1 : w0; }

    // Nice to have
    inline size_t size() const { return clause.size(); }
    inline const Literal& at(size_t i) const { return clause[i]; }

    const std::vector<Literal>& getClause() const;

    friend std::ostream& operator<<(std::ostream& os, const Clause& clause);

    void setLBD(int lbdVal);
    int getLBD() const;

    int computeLBD(const Trail& trail) const;

    double getActivity() const {
        return activity;
    }

    void bumpActivity(double inc) {
        activity += inc;
    }

    void decayActivity(double decay) {
        activity *= decay;
    }
};

#endif // CLAUSE_H