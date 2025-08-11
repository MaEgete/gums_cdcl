#include "Literal.h"

Literal::Literal(const int v, const bool neg) : var{v}, negated{neg} {}

int  Literal::getVar()     const { return this->var; }
bool Literal::isNegated()  const { return this->negated; }

int Literal::toInt() const { return negated ? -var : var; }

std::ostream& operator<<(std::ostream& os, const Literal& lit) {
    os << (lit.negated ? "¬" : "") << "x" << lit.var;
    return os;
}

bool Literal::operator==(const Literal& other) const {
    return var == other.var && negated == other.negated;
}