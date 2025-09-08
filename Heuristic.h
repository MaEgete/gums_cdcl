

#ifndef HEURISTIC_H
#define HEURISTIC_H

#include <set>
#include <random>
#include <vector>
#include "Trail.h"
#include "Clause.h"

class Heuristic {
private:
    // Menge aller aktuell unbelegten Variablen (für Random-Heuristik)
    std::set<int> unassignedVars;

    // Zufallsgenerator (für Random). Einmalig statisch initialisiert.
    static inline std::mt19937_64 rng = std::mt19937_64(std::random_device{}());

    // Jeroslow-Wang: getrennte Scores für positive/negative Literale.
    // Index 0 bleibt unbenutzt, gültig sind 1..numVars.
    // std::vector<double> jwScores; // (früherer gemeinsamer Score; ungenutzt)
    std::vector<double> jwPosScores;
    std::vector<double> jwNegScores;

    // --- VSIDS-Zustand ---
    // Aktivitäten je Variable (1..n)
    std::vector<double> vsidsActivity;
    // Max-Heap über Variablen-IDs nach Aktivität
    std::vector<int>    vsidsHeap;
    // Position einer Variable im Heap (-1, wenn nicht enthalten)
    std::vector<int>    vsidsPos;
    // Inkrement und Zerfallsfaktor für EVSIDS
    double              vsidsVarInc  = 1.0;
    double              vsidsVarDecay = 0.95;

public:
    // Zufalls-Seed setzen (reproduzierbare Random-Auswahl)
    void setSeed(uint64_t s);

    // Random-Heuristik: Grundinitialisierung (alle Variablen als unbelegt eintragen)
    void initialize(int numVars);

    // Random-Heuristik: wählt eine unbelegte Variable zufällig aus
    int  pickRandomVar();

    // Random-Heuristik: unbelegte Variablen anhand des Trails neu bestimmen
    void update(const Trail& trail, int numVars);

    // Jeroslow-Wang: wählt Variable und gibt zusätzlich eine empfohlene Polarität zurück
    std::pair<int, bool> pickJeroslowWangVar(const std::vector<int>& assignment) const;

    // Jeroslow-Wang: Scores einmalig aus allen Klauseln berechnen
    void initializeJeroslowWang(const std::vector<Clause>& clauses, int numVars);

    // Jeroslow-Wang: Scores inkrementell für eine neue Klausel updaten
    void updateJeroslowWang(const Clause& newClause);

    // --- VSIDS API ---
    // VSIDS-Strukturen zurücksetzen/initialisieren
    void initializeVSIDS(int numVars, double decay = 0.95);

    // Aktivität einer Variable erhöhen (mit Rescale-Schutz)
    void vsidsBump(int v);

    // Zerfall des Inkrements (klassisches EVSIDS: varInc /= decay)
    void vsidsDecayInc();

    // Nach Backtrack: Variable wieder in den Heap einfügen, falls nicht enthalten
    void onBacktrackUnassign(int v);

    // --- VSIDS-Hilfsfunktionen (Heap-Operationen) ---
    // Spitze des Heaps lesen (Variablen-ID) oder -1, wenn leer
    int  heapTop() const;
    // Spitze entfernen und Heap reparieren
    void heapPop();
    // Heap nach oben korrigieren ab Index i
    void heapifyUp(int i);
    // Heap nach unten korrigieren ab Index i
    void heapifyDown(int i);
    // Variable in den Heap einfügen
    void heapInsert(int v);
    // Schlüssel von v wurde erhöht -> ggf. nach oben schieben
    void heapIncreaseKey(int v);
};

#endif // HEURISTIC_H