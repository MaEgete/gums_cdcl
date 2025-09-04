// CNFParser.cpp
// --------------
// Liest eine DIMACS-CNF-Datei ein und baut daraus Klausel-Objekte auf.
// Unterstützt Kommentare (Zeilen mit 'c') und den Header "p cnf <vars> <clauses>".
// Jede Klausel endet in der Datei mit einer 0.

#include "CNFParser.h"

#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>
#include <limits>

// Pfad speichern
CNFParser::CNFParser(const std::string_view path) : path{path} {}

bool CNFParser::readFile() {
    std::cout << "Datei einlesen..." << std::flush;

    std::ifstream ifs{this->path};

    // Datei lässt sich nicht öffnen -> Fehler
    if (!ifs.is_open()) {
        std::cerr << "Fehler: Datei konnte nicht geöffnet werden! Pfad: "
                  << this->path << std::endl;
        return false;
    }

    // Laufende Zähler und Puffer
    int clauseCount = 0;     // tatsächlich gelesene Klauseln
    std::set<int> varsCount; // Menge der vorkommenden Variablen-IDs (zur Plausibilitätsprüfung)
    int numVars    = 0;      // Anzahl Variablen laut Header
    int numClauses = 0;      // Anzahl Klauseln laut Header

    std::string tok;                   // aktuelles Token
    std::vector<Literal> current;      // sammelt Literale bis zur "0" (Ende der Klausel)

    // Haupt-Lese-Schleife: tokenweise
    while (ifs >> tok) {
        // Kommentarzeile: alles bis Zeilenende ignorieren
        if (!tok.empty() && tok[0] == 'c') {
            ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        // Header-Zeile: "p cnf <numVars> <numClauses>"
        if (!tok.empty() && tok[0] == 'p') {
            std::string cnf;
            ifs >> cnf >> numVars >> numClauses;
            // Rest der Zeile ignorieren, falls noch was kommt
            ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        // Ab hier erwarten wir Integer-Tokens (Literal oder 0)
        int lit = std::stoi(tok);

        // 0 = Klauselabschluss -> Klausel fertigstellen und speichern
        if (lit == 0) {
            if (!current.empty()) {
                Clause clause(std::move(current));
                clause.initWatchesDefault();           // Standard-Watches setzen
                this->clauses.push_back(std::move(clause));
                ++clauseCount;
                current.clear();                       // Puffer leeren für die nächste Klausel
            }
            continue;
        }

        // Normales Literal: Vorzeichen = Negation, Betrag = Variable
        int  v   = std::abs(lit);
        bool neg = (lit < 0);
        current.emplace_back(v, neg);
        varsCount.insert(v); // für Plausibilitätscheck (maxVarSeen)
    }

    // Dateiende erreicht:
    // Falls die letzte Klausel NICHT mit 0 abgeschlossen wurde, verwerfen wir sie (optional warnen).
    if (!current.empty()) {
        std::cerr << "Warnung: Letzte Klausel endete nicht mit 0 und wurde ignoriert.\n";
        current.clear();
    }

    // Maximale tatsächlich verwendete Variable bestimmen (zur Plausibilitätsprüfung)
    int maxVarSeen = varsCount.empty() ? 0 : *std::ranges::max_element(varsCount);

    // Plausibilitätscheck der Variablenanzahl:
    //  - DIMACS erlaubt, dass im Header mehr Variablen angegeben sind als genutzt
    //  - Fehler nur, wenn keine Variable gelesen wurde oder maxVarSeen > numVars
    if (varsCount.empty() || maxVarSeen > numVars) {
        std::cerr << "Variablen stimmen nicht überein!\n"
                     "Max gefundene Variable: " << maxVarSeen
                  << ", Header (deklarierte Variablen): " << numVars << std::endl;
        this->clauses.clear();
        return false;
    }

    // Für nachgelagerte Strukturen zählt die Header-Angabe
    this->numVariables = numVars;

    // Plausibilitätscheck der Klauselanzahl
    if (clauseCount != numClauses) {
        std::cerr << "Anzahl der Klauseln stimmen nicht überein!\n"
                     "Angegebene Klauseln: " << numClauses
                  << ", Tatsächliche Klauseln: " << clauseCount << std::endl;
        this->clauses.clear();
        return false;
    }

    return true;
}

// Referenz auf die Klauselliste zurückgeben (damit der Solver sie übernehmen kann)
std::vector<Clause>& CNFParser::getClauses() {
    return this->clauses;
}

// Anzahl der Variablen liefern (aus dem Header)
int CNFParser::getNumVariables() const {
    return this->numVariables;
}