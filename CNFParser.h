/*
CNFParser.h
  -----------
  Liest eine CNF-Datei im DIMACS-Format ein und wandelt diese in Klauseln um,
  die der Solver weiterverarbeiten kann.

  Aufgaben:
    - Pfad zur Datei speichern
    - Datei einlesen und Parsen (DIMACS-Standard: Kommentare, Header, Klauseln)
    - Zugriff auf Klauseln und Anzahl der Variablen ermöglichen
*/

#ifndef CNFPARSER_H
#define CNFPARSER_H

#include <filesystem>
#include <vector>

#include "Literal.h"
#include "Clause.h"

class CNFParser {
private:
    // Pfad zur CNF-Datei
    std::filesystem::path path;

    // Alle eingelesenen Klauseln
    std::vector<Clause> clauses;

    // Anzahl der in der Datei deklarierten Variablen
    int numVariables{};

public:
    // Konstruktor mit Dateipfad
    explicit CNFParser(const std::string_view path);

    // Liest die Datei und baut die Klauseln auf
    bool readFile();

    // Zugriff auf die eingelesenen Klauseln
    std::vector<Clause>& getClauses();

    // Anzahl der Variablen zurückgeben
    int getNumVariables() const;
};

#endif // CNFPARSER_H