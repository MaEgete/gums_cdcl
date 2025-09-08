

#include <iostream>
#include <vector>
#include <chrono>
#include <optional>
#include <string>
#include <cstring> // für std::strncmp, std::strlen

#include "Literal.h"
#include "Clause.h"
#include "Trail.h"
#include "CNFParser.h"
#include "Solver.h"

// Kleine Hilfsfunktion: holt den Wert zu einem Schlüssel aus den CLI-Argumenten
// Funktioniert sowohl für Formen wie "--key=value" als auch "--keyvalue" (falls so übergeben)
static std::optional<std::string> getArgValue(int argc, char** argv, const char* key) {
    const std::string prefix = std::string(key) + "=";
    for (int i = 1; i < argc; ++i) {
        // exakte Übereinstimmung mit dem Schlüssel, dann direkt den Rest der Zeichenkette als Wert nehmen
        if (std::strncmp(argv[i], key, std::strlen(key)) == 0) {
            return std::string(argv[i] + std::strlen(key));
        }
        // oder Präfix-Variante: "--key=value"
        if (std::string(argv[i]).rfind(prefix, 0) == 0) {
            return std::string(argv[i] + prefix.size());
        }
    }
    return std::nullopt;
}

int main(int argc, char** argv) {

    // Kleiner ASCII-Banner in Grün (ANSI-Farbcodes), rein kosmetisch
    std::cout << "\033[32m" << R"(

     ██████╗ ██╗   ██╗███╗   ███╗███████╗      ███████╗ ██████╗ ██╗    ██╗   ██╗███████╗██████╗
    ██╔════╝ ██║   ██║████╗ ████║██╔════╝      ██╔════╝██╔═══██╗██║    ██║   ██║██╔════╝██╔══██╗
    ██║  ███╗██║   ██║██╔████╔██║███████╗█████╗███████╗██║   ██║██║    ██║   ██║█████╗  ██████╔╝
    ██║   ██║██║   ██║██║╚██╔╝██║╚════██║╚════╝╚════██║██║   ██║██║    ╚██╗ ██╔╝██╔══╝  ██╔══██╗
    ╚██████╔╝╚██████╔╝██║ ╚═╝ ██║███████║      ███████║╚██████╔╝███████╗╚████╔╝ ███████╗██║  ██║
     ╚═════╝  ╚═════╝ ╚═╝     ╚═╝╚══════╝      ╚══════╝ ╚═════╝ ╚══════╝ ╚═══╝  ╚══════╝╚═╝  ╚═╝

    )" << "\033[0m" << std::endl;

    std::cout << "Ich bin der CDCL-SAT-Solver Gums" << std::endl;

    // --- CLI-Optionen (Defaults) ---
    HeuristicType ht = HeuristicType::VSIDS;         // Standard-Heuristik
    uint64_t seed = 0;                                // 0 = kein fester Seed
    std::string pfad = "../examples/stundenplan.cnf"; // Standardpfad zur CNF-Datei

    // --cnf=PATH  (Pfad zur CNF-Datei überschreiben)
    if (auto a = getArgValue(argc, argv, "--cnf")) {
        pfad = *a;
    }

    // --heuristic=jw|random|vsids  (Heuristik auswählen)
    if (auto h = getArgValue(argc, argv, "--heuristic")) {
        std::string v = *h;
        for (auto& c : v) c = std::tolower(c);
        if (v == "random") {
            ht = HeuristicType::RANDOM;
        }
        else if (v == "jw" || v == "jeroslow" || v == "jeroslow_wang") {
            ht = HeuristicType::JEROSLOW_WANG;
        }
        else if (v == "vsids") {
            ht = HeuristicType::VSIDS;
        }
        // ansonsten bleibt der Default (VSIDS) bestehen
    }

    // --seed=NUMBER  (Seed für Random-Heuristik setzen)
    if (auto s = getArgValue(argc, argv, "--seed")) {
        try {
            seed = std::stoull(*s);
        }
        catch (...) {
            seed = 0; // ungültiger Wert -> zurück zum Default
        }
    }

    // Parser für die DIMACS-CNF-Datei anlegen
    CNFParser parser{pfad};

    // Zeitmessung für das Einlesen der Datei
    auto start = std::chrono::high_resolution_clock::now();

    if (parser.readFile()) {

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        std::cout << duration_seconds << " Sekunden" << std::endl;

        // Gelesene Klauseln und Variablenanzahl übernehmen
        auto clauses = parser.getClauses();
        int numVars = parser.getNumVariables();

        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Gums-Solver wird gestartet" << std::endl;

        // Solver erstellen, Heuristik setzen (Default ist VSIDS)
        Solver solver{numVars};
        solver.setHeuristic(ht);
        if (seed != 0) {
            solver.setHeuristicSeed(seed);
        }

        // Klauseln hinzufügen (Zeitmessung optional)
        start = std::chrono::high_resolution_clock::now();
        for (const auto& clause : clauses) {
            solver.addClause(clause);
        }
        end = std::chrono::high_resolution_clock::now();
        duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        std::cout << "Klauseln hinzufügen: " << duration_seconds << " Sekunden" << std::endl;

        // Lösen starten
        std::cout << "Solving..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        bool result = solver.solve();
        end = std::chrono::high_resolution_clock::now();

        // Laufzeiten ausgeben (ms / s / min)
        auto duration_milli   = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        duration_seconds      = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        auto duration_minutes = std::chrono::duration_cast<std::chrono::minutes>(end - start);

        // Ergebnis + Statistiken
        if (result) {
            solver.printModel();
            solver.printStats();
            std::cout << "SATISFIABLE" << std::endl;
        } else {
            solver.printStats();
            std::cout << "UNSATISFIABLE" << std::endl;
        }

        // Platzhalter für die schriftliche Dokumentation/Methodik
        std::cout << "Methodik noch klar dokumentieren" << std::endl;

        // Übersichtliche Laufzeit-Zusammenfassung
        std::cout << std::string(20, '-') << std::endl;
        std::cout << "Laufzeit:\n"
                  << " - " << duration_milli.count()   << " ms\n"
                  << " - " << duration_seconds.count() << " seconds\n"
                  << " - " << duration_minutes.count() << " minutes\n";
    } else {
        // Parser konnte die Datei nicht lesen (Fehlerhinweis erfolgt im Parser selbst)
        // Frühzeitiger Abbruch
        return 1;
    }

    return 0;
}