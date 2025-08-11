#ifndef CNFPARSER_H
#define CNFPARSER_H

#include <filesystem>
#include <vector>

#include "Literal.h"
#include "Clause.h"

class CNFParser {
private:
    std::filesystem::path path;
    std::vector<Clause>   clauses;
    int                   numVariables{};

public:
    explicit CNFParser(const std::string_view path);

    bool readFile();

    std::vector<Clause>& getClauses();
    int getNumVariables() const;
};

#endif // CNFPARSER_H