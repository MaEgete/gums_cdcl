#ifndef LITERAL_H
#define LITERAL_H

#include <iostream>

class Literal {
private:
    int  var;
    bool negated;

public:
    Literal(const int v = 0, const bool neg = false);

    int  getVar()     const;
    bool isNegated()  const;

    int toInt() const;

    friend std::ostream& operator<<(std::ostream& os, const Literal& lit);

    bool operator==(const Literal& other) const;
};

#endif // LITERAL_H