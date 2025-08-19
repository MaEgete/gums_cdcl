#include <iostream>
#include <vector>
#include <chrono>

#include "Literal.h"
#include "Clause.h"
#include "Trail.h"
#include "CNFParser.h"
#include "Solver.h"


int main() {

    std::cout << "Ich bin der CDCL-SAT-Solver Gums" << std::endl;

    std::string pfad = "../examples/stundenplan.cnf"; // ggf. anpassen

    CNFParser parser{pfad};

    if (parser.readFile()) {
        auto clauses = parser.getClauses();
        int numVars = parser.getNumVariables();

        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Gums-Solver wird gestartet" << std::endl;

        Solver solver{numVars};
        for (const auto& clause : clauses) {
            solver.addClause(clause);
        }

        std::cout << "Solving..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        bool result = solver.solve();
        auto end = std::chrono::high_resolution_clock::now();

        auto duration_milli   = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        auto duration_minutes = std::chrono::duration_cast<std::chrono::minutes>(end - start);

        if (result) {
            solver.printModel();
            std::cout << "SATISFIABLE" << std::endl;
            solver.printStats();
        } else {
            std::cout << "UNSATISFIABLE" << std::endl;
        }
        std::cout << std::string(20, '-') << std::endl;
        std::cout << "Laufzeit:\n"
                  << " - " << duration_milli.count()   << " ms\n"
                  << " - " << duration_seconds.count() << " seconds\n"
                  << " - " << duration_minutes.count() << " minutes\n";
    }

    return 0;
}