#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <cstring>   // std::strncmp, std::strlen
#include <cctype>    // std::tolower
#include <filesystem>

#include "Literal.h"
#include "Clause.h"
#include "Trail.h"
#include "CNFParser.h"
#include "Solver.h"

// ------------------------------------------------------------
// Hilfsfunktionen für CLI
// ------------------------------------------------------------

static std::optional<std::string> getArgValue(int argc, char** argv, const char* key) {
    // Unterstützt "--key=value" und die alte Kurzform "--keyvalue"
    const std::string prefix = std::string(key) + "=";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i]; // CLI-Argument als std::string speichern

        // bevorzugt die Form "--key=value"
        if (arg.rfind(prefix, 0) == 0) { // prüft, ob arg mit prefix beginnt (ab Position 0)
            return arg.substr(prefix.size()); // extrahiert den Teil nach dem Prefix
        }

        // alte Kurzform "--keyvalue"
        if (std::strncmp(argv[i], key, std::strlen(key)) == 0) { // vergleicht die ersten strlen(key) Zeichen
            return std::string(argv[i] + std::strlen(key));      // gibt den Rest von argv[i] nach dem key zurück
        }
    }

    return std::nullopt; // kein passendes Argument gefunden
}

// Teilt eine kommaseparierte Liste in Tokens
static std::vector<std::string> splitCommaList(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(',', start); // Komma ab Position start suchen
        std::string token = s.substr(start, (pos == std::string::npos) ? std::string::npos : pos - start); // Token (substring) extrahieren
        if (!token.empty()) {
            out.push_back(token); // Zum Vector out hinzufügen
        }
        if (pos == std::string::npos) break;

        start = pos + 1;
    }
    return out;
}

// ------------------------------------------------------------
// Heuristik-Parsing
// ------------------------------------------------------------

static std::optional<HeuristicType> heuristicFromString(std::string v) {
    for (auto& c : v) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (v == "vsids") {
        return HeuristicType::VSIDS;
    }
    if (v == "jw" || v == "jeroslow" || v == "jeroslow_wang") {
        return HeuristicType::JEROSLOW_WANG;
    }
    if (v == "random") {
        return HeuristicType::RANDOM;
    }
    return std::nullopt;
}

static const char* heuristicName(HeuristicType h) {
    switch (h) {
        case HeuristicType::VSIDS:          return "VSIDS";
        case HeuristicType::JEROSLOW_WANG:  return "JEROSLOW_WANG";
        case HeuristicType::RANDOM:         return "RANDOM";
    }
    return "UNKNOWN";
}

// Sammelt alle Heuristiken aus den CLI-Argumenten.
// Akzeptiert:
//   --heuristic=vsids
//   --heuristic=vsids,jw,random
//   --heuristicvsids   (alte Kurzform)
//   --heuristicjw
//   --heuristicrandom
static std::vector<HeuristicType> collectHeuristicsFromCLI(int argc, char** argv) {
    std::vector<HeuristicType> hs;

    // 1) Alle Vorkommen von "--heuristic=..." abgrasen
    const std::string keyEq = "--heuristic=";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind(keyEq, 0) == 0) { // prüft, ob arg mit keyEq beginnt (ab Position 0)
            auto vals = splitCommaList(arg.substr(keyEq.size())); // Argumente ab Position keyEq.size() splitten
            for (auto& v : vals) {
                if (auto ht = heuristicFromString(v)) {
                    hs.push_back(*ht);
                } else {
                    std::cerr << "Fehler: Unbekannte Heuristik \"" << v << "\".\n";
                    std::exit(1); // oder return {}; falls du den Fehler nur weiterreichen willst
                }
            }
        }
    }

    // 2) Alte Kurzform: "--heuristicvsids", "--heuristicjw", "--heuristicrandom"
    // aber NUR wenn KEIN '=' enthalten ist (sonst ist es schon von oben verarbeitet worden)
    const char* key = "--heuristic";
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], key, std::strlen(key)) == 0) {
            std::string suffix = std::string(argv[i] + std::strlen(key));

            // NEU: wenn der erste Buchstabe im Suffix ein '=' ist → überspringen
            if (!suffix.empty() && suffix[0] == '=') {
                continue; // wurde oben schon verarbeitet
            }

            if (!suffix.empty()) {
                if (auto ht = heuristicFromString(suffix)) {
                    hs.push_back(*ht);
                } else {
                    std::cerr << "Fehler: Unbekannte Heuristik \"" << suffix << "\".\n";
                    std::exit(1);
                }
            }
        }
    }

    // Default, falls nichts angegeben wurde
    if (hs.empty()) {
        hs.push_back(HeuristicType::VSIDS);
    }
    return hs;
}

// ------------------------------------------------------------

int main(int argc, char** argv) {
    // Kleiner ASCII-Banner in Grün (kosmetisch)
    std::cout << "\033[32m" << R"(

     ██████╗ ██╗   ██╗███╗   ███╗███████╗      ███████╗ ██████╗ ██╗    ██╗   ██╗███████╗██████╗
    ██╔════╝ ██║   ██║████╗ ████║██╔════╝      ██╔════╝██╔═══██╗██║    ██║   ██║██╔════╝██╔══██╗
    ██║  ███╗██║   ██║██╔████╔██║███████╗█████╗███████╗██║   ██║██║    ██║   ██║█████╗  ██████╔╝
    ██║   ██║██║   ██║██║╚██╔╝██║╚════██║╚════╝╚════██║██║   ██║██║    ╚██╗ ██╔╝██╔══╝  ██╔══██╗
    ╚██████╔╝╚██████╔╝██║ ╚═╝ ██║███████║      ███████║╚██████╔╝███████╗╚████╔╝ ███████╗██║  ██║
     ╚═════╝  ╚═════╝ ╚═╝     ╚═╝╚══════╝      ╚══════╝ ╚═════╝ ╚══════╝ ╚═══╝  ╚══════╝╚═╝  ╚═╝

    )" << "\033[0m" << std::endl;

    std::cout << "Ich bin der CDCL-SAT-Solver Gums" << std::endl;

    // --- CLI-Defaults ---
    std::string cnfPath = "../examples/Sudoku1_ohne.cnf";
    uint64_t seed = 0; // 0 = kein fixer Seed
    std::string statsCsvFile = "default.csv";

    // --stats=PATH
    if (auto s = getArgValue(argc, argv, "--stats")) {
        statsCsvFile = *s;
    }

    // Heuristiken werden unten gesammelt

    // --cnf=PATH
    if (auto a = getArgValue(argc, argv, "--cnf")) {
        cnfPath = *a;
    }

    // --seed=NUMBER
    if (auto s = getArgValue(argc, argv, "--seed")) {
        try {
            seed = std::stoull(*s); // string to unsigned long long
        } catch (...) {
            seed = 0;
        }
    }

    // Mehrere Heuristiken einsammeln
    std::vector<HeuristicType> heuristics = collectHeuristicsFromCLI(argc, argv);


    // Parser anlegen + Einlesezeit messen
    std::cout << "Arbeitsverzeichnis: " << std::filesystem::current_path() << "\n";
    std::cout << "CNF-Pfad (übergeben): " << cnfPath << "\n";
    std::error_code _fs_ec;
    auto _abs = std::filesystem::weakly_canonical(std::filesystem::path(cnfPath), _fs_ec);
    if (!_fs_ec) {
        std::cout << "CNF-Pfad (aufgelöst): " << _abs << "\n";
        cnfPath = _abs.string();
    } else {
        std::cerr << "Warnung: Pfad konnte nicht vollständig aufgelöst werden: "
                  << cnfPath << "\n"
                  << "Fehlermeldung: " << _fs_ec.message() << "\n";
    }
    std::cout.flush();



    CNFParser parser{cnfPath};
    auto t_read_start = std::chrono::high_resolution_clock::now();

    if (!parser.readFile()) {
        std::cerr << "Fehler: Konnte CNF nicht lesen: " << cnfPath << std::endl;
        return 1;
    }

    auto t_read_end = std::chrono::high_resolution_clock::now();
    auto read_seconds = std::chrono::duration_cast<std::chrono::seconds>(t_read_end - t_read_start);
    std::cout << "Einlesen: " << read_seconds.count() << " Sekunden\n";

    // Klauseln/Variablen aus Parser übernehmen
    const auto clauses = parser.getClauses();
    const int numVars  = parser.getNumVariables();

    std::cout << std::string(40, '-') << "\n";
    std::cout << "Starte Runs für " << heuristics.size() << " Heuristik(en)\n";

    // Für jede Heuristik: eigener Solver, Clauses hinzufügen, solve(), Stats drucken
    for (const auto& h : heuristics) {
        std::cout << std::string(40, '=') << "\n";
        std::cout << "Heuristik: " << heuristicName(h) << "\n";

        Solver solver{numVars};
        solver.setHeuristic(h);
        if (seed != 0) {
            solver.setHeuristicSeed(seed);
        }

        // Klauseln hinzufügen (Zeitmessung optional)
        auto t_add_start = std::chrono::high_resolution_clock::now();
        for (const auto& c : clauses) {
            solver.addClause(c);
        }
        auto t_add_end = std::chrono::high_resolution_clock::now();
        auto add_seconds = std::chrono::duration_cast<std::chrono::seconds>(t_add_end - t_add_start);
        std::cout << "Klauseln hinzufügen: " << add_seconds.count() << " Sekunden\n";

        // Lösen
        std::cout << "Solving...\n";
        auto t_solve_start = std::chrono::high_resolution_clock::now();
        bool sat = solver.solve();
        auto t_solve_end   = std::chrono::high_resolution_clock::now();

        // Laufzeiten
        auto dt_ms  = std::chrono::duration_cast<std::chrono::milliseconds>(t_solve_end - t_solve_start);
        auto dt_s   = std::chrono::duration_cast<std::chrono::seconds>(t_solve_end - t_solve_start);
        auto dt_min = std::chrono::duration_cast<std::chrono::minutes>(t_solve_end - t_solve_start);

        // Ergebnis + Stats
        if (sat) {
            std::cout << "SATISFIABLE\n";
            solver.printModel();
            solver.printStats();
        } else {
            std::cout << "UNSATISFIABLE\n";
            solver.printStats();
        }

        // Kompakte Zusammenfassung / gut für Parser (optional erweiterbar)
        std::cout << std::string(20, '-') << "\n";
        std::cout << "Laufzeit (" << heuristicName(h) << "):\n"
                  << " - " << dt_ms.count()  << " ms\n"
                  << " - " << dt_s.count()   << " seconds\n"
                  << " - " << dt_min.count() << " minutes\n";

        // Einheitliche RESULT-Zeile (praktisch für CSV-Parser)
        // time_s = bei UNSAT/SAT reale Zeit in Sekunden
        std::cout << "RESULT,heuristic=" << heuristicName(h)
                  << ",seed=" << seed
                  << ",instance=" << cnfPath
                  << ",solved=" << (sat ? 1 : 0)
                  << ",time_s=" << dt_s.count()
                  << std::endl;

        solver.exportStats(statsCsvFile);
    }

    return 0;
}