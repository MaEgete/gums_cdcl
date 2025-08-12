#include "CNFParser.h"

#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>
#include <limits>

CNFParser::CNFParser(const std::string_view path) : path{path} {}

bool CNFParser::readFile() {
    std::cout << "Datei einlesen..." << std::endl;
    std::ifstream ifs{this->path};

    if (!ifs.is_open()) {
        std::cerr << "Fehler: Datei konnte nicht geöffnet werden! Pfad: "
                  << this->path << std::endl;
        return false;
    }

    int clauseCount = 0; // Anzahl an Klauseln überprüfen
    std::set<int> varsCount; // Anzahl an Variablen überprüfen
    int numVars     = 0; // Anzahl an Variablen aus der Datei
    int numClauses  = 0; // Anzahl an Klauseln aus der Datei

    std::string tok;
    std::vector<Literal> current; // sammelt Literale bis zur 0 (Klauselabschluss)

    while (ifs >> tok) {
        if (!tok.empty() && tok[0] == 'c') {
            // Kommentar: Rest der Zeile überspringen
            ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (!tok.empty() && tok[0] == 'p') {
            // Header: "p cnf <numVars> <numClauses>"
            std::string cnf;
            ifs >> cnf >> numVars >> numClauses;
            // Rest der Zeile ignorieren (robust ggü. trailing content)
            ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        // Ab hier erwarten wir Integer-Tokens (Literal oder 0)
        int lit = std::stoi(tok);
        if (lit == 0) {
            if (!current.empty()) {
                Clause clause(std::move(current));
                clause.initWatchesDefault();
                this->clauses.push_back(std::move(clause));
                ++clauseCount;
                current.clear();
            }
            continue;
        }

        int v = std::abs(lit);
        bool neg = (lit < 0);
        current.emplace_back(v, neg);
        varsCount.insert(v);
    }

    // Dateiende: Falls eine Klausel nicht mit 0 abgeschlossen wurde, ignorieren wir den Rest (optional Warnung)
    if (!current.empty()) {
        std::cerr << "Warnung: Letzte Klausel endete nicht mit 0 und wurde ignoriert.\n";
        current.clear();
    }

    // Maximale tatsächlich verwendete Variable bestimmen
    int maxVarSeen = varsCount.empty() ? 0 : *std::ranges::max_element(varsCount);

    // Sicherheitsabfrage - Abgleich von Werten
    // DIMACS erlaubt, dass der Header mehr Variablen deklariert als tatsächlich verwendet.
    // Fehler ist nur, wenn maxVarSeen > numVars oder gar keine Variable gelesen wurde.
    if (varsCount.empty() || maxVarSeen > numVars) {
        std::cerr << "Variablen stimmen nicht überein!\n"
                     "Max gefundene Variable: " << maxVarSeen
                  << ", Header (deklarierte Variablen): " << numVars << std::endl;
        this->clauses.clear();
        return false;
    }

    // Für nachgelagerte Strukturen ist der Header-Wert maßgeblich
    this->numVariables = numVars;

    // Sicherheitsabfrage - Abgleich von Werten
    if (clauseCount != numClauses) {
        std::cerr << "Anzahl der Klauseln stimmen nicht überein!\n"
                     "Angegebene Klauseln: " << numClauses
                  << ", Tatsächliche Klauseln: " << clauseCount << std::endl;
        this->clauses.clear();
        return false;
    }

    return true;
}

std::vector<Clause>& CNFParser::getClauses() {
    return this->clauses;
}

int CNFParser::getNumVariables() const {
    return this->numVariables;
}