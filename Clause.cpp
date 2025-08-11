#include "Clause.h"

Clause::Clause(std::vector<Literal> cla) : clause{std::move(cla)} {}

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