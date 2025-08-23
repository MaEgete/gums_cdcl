#include <iostream>
#include <vector>
#include <chrono>
#include <optional>
#include <string>

#include "Literal.h"
#include "Clause.h"
#include "Trail.h"
#include "CNFParser.h"
#include "Solver.h"

static std::optional<std::string> getArgValue(int argc, char** argv, const char* key) {
    const std::string prefix = std::string(key) + "=";
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], key, std::strlen(key)) == 0) {
            return std::string(argv[i] + std::strlen(key));
        }
        if (std::string(argv[i]).rfind(prefix, 0) == 0) {
            return std::string(argv[i] + prefix.size());
        }
    }
    return std::nullopt;
}


int main(int argc, char** argv) {

    std::cout << "\033[32m" << R"(

     ██████╗ ██╗   ██╗███╗   ███╗███████╗      ███████╗ ██████╗ ██╗    ██╗   ██╗███████╗██████╗
    ██╔════╝ ██║   ██║████╗ ████║██╔════╝      ██╔════╝██╔═══██╗██║    ██║   ██║██╔════╝██╔══██╗
    ██║  ███╗██║   ██║██╔████╔██║███████╗█████╗███████╗██║   ██║██║    ██║   ██║█████╗  ██████╔╝
    ██║   ██║██║   ██║██║╚██╔╝██║╚════██║╚════╝╚════██║██║   ██║██║    ╚██╗ ██╔╝██╔══╝  ██╔══██╗
    ╚██████╔╝╚██████╔╝██║ ╚═╝ ██║███████║      ███████║╚██████╔╝███████╗╚████╔╝ ███████╗██║  ██║
     ╚═════╝  ╚═════╝ ╚═╝     ╚═╝╚══════╝      ╚══════╝ ╚═════╝ ╚══════╝ ╚═══╝  ╚══════╝╚═╝  ╚═╝

    )" << "\033[0m" << std::endl;


    std::cout << "Ich bin der CDCL-SAT-Solver Gums" << std::endl;


    // --- CLI-Optionen ---
    // Defaults
    HeuristicType ht = HeuristicType::VSIDS;
    uint64_t seed = 0; // 0 = kein gefixter seed
    std::string pfad = "../examples/stundenplan.cnf"; // default path

    // --cnf=PATH
    if (auto a = getArgValue(argc, argv, "--cnf")) {
        pfad = *a;
    }

    // --heuristics=jw|random
    if (auto h = getArgValue(argc, argv, "--heuristic")) {
        std::string v = *h;
        for (auto& c : v) {
            c = std::tolower(c);
        }
        if (v == "random") {
            ht = HeuristicType::RANDOM;
        }
        else if (v == "jw" || v == "jeroslow" || v == "jeroslow_wang") {
            ht = HeuristicType::JEROSLOW_WANG;
        }
        else if (v == "vsids") {
            ht = HeuristicType::VSIDS;
        }
    }

    // --seed=NUMBER
    if (auto s = getArgValue(argc, argv, "--seed")) {
        try {
            seed = std::stoull(*s);
        }
        catch (...) {
            seed = 0;
        }
    }

    CNFParser parser{pfad};

    if (parser.readFile()) {
        auto clauses = parser.getClauses();
        int numVars = parser.getNumVariables();

        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Gums-Solver wird gestartet" << std::endl;

        Solver solver{numVars};
        solver.setHeuristic(ht);
        if (seed != 0) {
            solver.setHeuristicSeed(seed);
        }
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