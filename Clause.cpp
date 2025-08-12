#include "Clause.h"

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