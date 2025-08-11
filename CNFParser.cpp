#include "CNFParser.h"

#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>

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

    std::string line;
    while (std::getline(ifs, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        if (line.empty() || line[0] == 'c') continue;

        if (line[0] == 'p') {
            std::istringstream iss(line);
            std::string tmp;
            iss >> tmp >> tmp >> numVars >> numClauses;
        } else {
            std::istringstream iss(line);
            int lit;
            std::vector<Literal> clauseLits; // Literal-Vector erstellen
            while (iss >> lit && lit != 0) {
                int lit_abs = std::abs(lit); // Variable aus dem Literal extrahieren
                clauseLits.emplace_back(lit_abs, lit < 0); // Eintrag in Literal-Vector erstellen
                varsCount.insert(lit_abs); // Anzahl an Variablen überprüfen
            }
            if (!clauseLits.empty()) { // Sicherheitsabfrage
                Clause clause(clauseLits); // Aus Literal-Vector die Klausel erstellen
                clauseCount++; // Anzahl an Klauseln überprüfen
                this->clauses.push_back(clause);
            }
        }
    }

    // Die Maximale Variable heraus extrahieren aus der Set
    numVariables = varsCount.empty() ? 0 : *std::ranges::max_element(varsCount);

    // Sicherheitsabfrage - Abgleich von Werten
    if (varsCount.empty() || numVariables != numVars) {
        std::cerr << "Variablen stimmen nicht überein!\n"
                     "Maximale angegebene Variable: " << numVariables
                  << ", Tatsächliche maximale Variable: " << numVars << std::endl;
        this->clauses.clear();
        return false;
    }

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