#include "Clause.h"

#include <unordered_set>

#include "Trail.h"

Clause::Clause(std::vector<Literal> cla) : clause{std::move(cla)} {}


void Clause::initWatchesDefault() {
    if (clause.empty()) { w0 = w1 = -1; return; }
    if (clause.size() == 1) { w0 = 0; w1 = 0; return; }
    w0 = 0;
    w1 = 1;
}


const std::vector<Literal>& Clause::getClause() const {
    return this->clause;
}

std::ostream& operator<<(std::ostream& os, const Clause& clause) {
    os << "{";
    for (size_t i = 0; i < clause.clause.size(); ++i) {
        os << clause.clause[i];
        if (i + 1 < clause.clause.size()) os << ", ";
    }
    os << "}";
    return os;
}

void Clause::setLBD(int lbdVal) {
    lbd = lbdVal;
}

int Clause::getLBD() const {
    return lbd;
}

int Clause::computeLBD(const Trail &trail) const {
    if (clause.empty()) return 0;
    if (clause.size() == 1) return 1;

    std::unordered_set<int> levels;
    levels.reserve(clause.size());

    if (clause.size() == 2) {
        int l0 = trail.getLevelOfVar(clause[0].getVar());
        int l1 = trail.getLevelOfVar(clause[1].getVar());
        return (l0 == l1) ? 1 : 2;
    }

    for (const auto& lit : clause) {
        levels.insert(trail.getLevelOfVar(lit.getVar()));
    }
    return static_cast<int>(levels.size());
}
