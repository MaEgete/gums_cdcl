#ifndef CLAUSE_H
#define CLAUSE_H

#include <utility>
#include <vector>
#include <iostream>
#include "Literal.h"

class Clause {
private:
    std::vector<Literal> clause;

public:
    explicit Clause(std::vector<Literal> cla = {});

    const std::vector<Literal>& getClause() const;

    friend std::ostream& operator<<(std::ostream& os, const Clause& clause);
};

#endif // CLAUSE_H