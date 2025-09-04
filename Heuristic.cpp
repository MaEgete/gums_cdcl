#include "Heuristic.h"
#include <iterator>
#include <cmath>
#include <stdexcept>
#include <limits>

// ======================= RANDOM HEURISTIK =======================

// Wählt zufällig eine unbelegte Variable aus der Menge unassignedVars.
int Heuristic::pickRandomVar() {
    if (unassignedVars.empty()) {
        // Wenn keine Variablen mehr unbelegt sind → Fehler
        throw std::runtime_error("Kann keine Variable von einem leeren Set nehmen");
    }
    // Verteilung: gleichmäßig über alle unbelegten Variablen
    std::uniform_int_distribution<size_t> dist(0, unassignedVars.size() - 1);
    size_t randomIndex = dist(rng);

    // Iterator auf die zufällige Position im Set bewegen
    auto it = unassignedVars.begin();
    std::advance(it, randomIndex);
    return *it; // Variable zurückgeben
}

// ======================= JEROSLOW-WANG HEURISTIK =======================

// Liefert ein Paar (Variable, Polarität).
// assignment: zeigt, welche Variablen bereits belegt sind.
std::pair<int, bool> Heuristic::pickJeroslowWangVar(const std::vector<int>& assignment) const {
    int bestVar  = -1;     // beste gefundene Variable
    double bestSum  = -1;  // Summe pos+neg
    double bestMax = -1;   // max(pos,neg)
    double bestPos = -1;   // pos-Score
    bool bestNeg = false;  // Polarität (true = negativ)

    int numVars  = static_cast<int>(assignment.size() - 1);

    // Alle Variablen durchgehen
    for (int v = 1; v <= numVars; ++v) {
        if (assignment[v] != -1) {
            continue; // überspringen, wenn schon belegt
        }

        // Scores der Variable
        const double pos = jwPosScores[v];
        const double neg = jwNegScores[v];
        const double sum = pos + neg;
        const double mx  = (pos > neg ? pos : neg);

        // Vergleichslogik (Champion-Prinzip)
        if (sum > bestSum
            || (sum == bestSum && mx > bestMax)
            || (sum == bestSum && mx == bestMax && pos > bestPos)
            || (sum == bestSum && mx == bestMax && pos == bestPos && (bestVar == -1 || v < bestVar))) {

            bestSum = sum;
            bestMax = mx;
            bestPos = pos;
            bestVar = v;
            bestNeg = (neg > pos); // Polarität = stärkere Seite
        }
    }

    // Fallback, falls keine Variable gefunden wurde
    if (bestVar == -1) {
        int v = -1;
        for (int i = 1; i <= numVars; ++i) {
            if (assignment[i] == -1) { v = i; break; }
        }
        if (v == -1) return {-1, false}; // alles belegt

        // Polarität nach den Scores bestimmen
        const bool negPol = (jwNegScores[v] > jwPosScores[v]);
        return {v, negPol};
    }

    return {bestVar, bestNeg};
}

// Initialisierung: Scores für alle Variablen aus den Klauseln berechnen
void Heuristic::initializeJeroslowWang(const std::vector<Clause>& clauses, int numVars) {
    jwPosScores.assign(numVars + 1, 0.0);
    jwNegScores.assign(numVars + 1, 0.0);

    for (const auto& clause : clauses) {
        // Gewicht = 2^(-Klauselgröße)
        double weight = std::pow(2.0, -static_cast<double>(clause.getClause().size()));
        for (const auto& lit : clause.getClause()) {
            if (lit.getVar() >= 1 && lit.getVar() < static_cast<int>(jwPosScores.size())) {
                if (lit.isNegated()) {
                    jwNegScores[lit.getVar()] += weight;
                } else {
                    jwPosScores[lit.getVar()] += weight;
                }
            }
        }
    }
}

// Update: neue Klausel in die JW-Scores einbeziehen
void Heuristic::updateJeroslowWang(const Clause& newClause) {
    double weight = std::pow(2.0, -static_cast<double>(newClause.getClause().size()));
    for (const auto& lit : newClause.getClause()) {
        if (lit.getVar() >= 1 && lit.getVar() < static_cast<int>(jwPosScores.size())) {
            if (lit.isNegated()) {
                jwNegScores[lit.getVar()] += weight;
            } else {
                jwPosScores[lit.getVar()] += weight;
            }
        }
    }
}

// ======================= RANDOM: Verwaltung der unbelegten Variablen =======================

// Unbelegte Variablen neu bestimmen (Trail = aktueller Zustand)
void Heuristic::update(const Trail& trail, int numVars) {
    unassignedVars.clear();
    for (int var = 1; var <= numVars; ++var) {
        if (!trail.isAssigned(var)) {
            unassignedVars.insert(var);
        }
    }
}

// Initialisierung: alle Variablen als unbelegt eintragen
void Heuristic::initialize(int numVars) {
    unassignedVars.clear();
    for (int v = 1; v <= numVars; ++v) unassignedVars.insert(v);
}

// Seed setzen für reproduzierbare Zufallszahlen
void Heuristic::setSeed(uint64_t s) {
    rng.seed(s);
}

// ======================= VSIDS =======================

// Initialisierung: Aktivitäten und Heap leeren
void Heuristic::initializeVSIDS(int numVars, double decay) {
    vsidsActivity.assign(numVars + 1, 0.0); // Aktivitäten
    vsidsPos.assign(numVars + 1, -1);       // Positionen im Heap
    vsidsHeap.clear();                      // Heap leeren
    vsidsVarInc   = 1.0;                    // Startwert für Inkrement
    vsidsVarDecay = decay;                  // Zerfallsrate
}

// Aktivität einer Variable erhöhen
void Heuristic::vsidsBump(int v) {
    if (v <= 0 || v >= static_cast<int>(vsidsActivity.size())) return;
    vsidsActivity[v] += vsidsVarInc;

    // Falls Aktivitäten zu groß werden: Rescaling
    if (vsidsActivity[v] > 1e100) {
        for (size_t u = 1; u < vsidsActivity.size(); ++u)
            vsidsActivity[u] *= 1e-100;
        vsidsVarInc *= 1e-100;
    }

    // Falls Variable im Heap → Schlüssel erhöhen
    if (vsidsPos[v] != -1) {
        heapIncreaseKey(v);
    } else {
        // Falls nicht im Heap → einfügen
        heapInsert(v);
    }
}

// Zerfall: varInc wird kleiner → alte Variablen „vergessen“
void Heuristic::vsidsDecayInc() {
    vsidsVarInc /= vsidsVarDecay;
}

// Nach Backtrack: Variable wieder in den Heap aufnehmen
void Heuristic::onBacktrackUnassign(int v) {
    if (v <= 0 || v >= static_cast<int>(vsidsPos.size())) return;
    if (vsidsPos[v] == -1) heapInsert(v);
}

// ======================= HEAP-OPERATIONEN (für VSIDS) =======================

// Spitze des Heaps zurückgeben (Variable mit höchster Aktivität)
int Heuristic::heapTop() const {
    return vsidsHeap.empty() ? -1 : vsidsHeap[0];
}

// Spitze entfernen
void Heuristic::heapPop() {
    if (vsidsHeap.empty()) return;
    int v = vsidsHeap[0];
    int last = vsidsHeap.back();
    vsidsHeap.pop_back();
    vsidsPos[v] = -1;

    if (!vsidsHeap.empty()) {
        vsidsHeap[0] = last;
        vsidsPos[last] = 0;
        heapifyDown(0); // Heap wieder herstellen
    }
}

// Nach oben „sieben“, falls Kind größer als Eltern
void Heuristic::heapifyUp(int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (vsidsActivity[vsidsHeap[parent]] >= vsidsActivity[vsidsHeap[i]]) break;
        std::swap(vsidsHeap[parent], vsidsHeap[i]);
        vsidsPos[vsidsHeap[parent]] = parent;
        vsidsPos[vsidsHeap[i]] = i;
        i = parent;
    }
}

// Nach unten „sieben“, falls Eltern kleiner als Kind
void Heuristic::heapifyDown(int i) {
    int n = static_cast<int>(vsidsHeap.size());
    while (true) {
        int left = 2*i + 1;
        int right = 2*i + 2;
        int largest = i;

        if (left < n && vsidsActivity[vsidsHeap[left]] > vsidsActivity[vsidsHeap[largest]]) largest = left;
        if (right < n && vsidsActivity[vsidsHeap[right]] > vsidsActivity[vsidsHeap[largest]]) largest = right;
        if (largest == i) break;

        std::swap(vsidsHeap[i], vsidsHeap[largest]);
        vsidsPos[vsidsHeap[i]] = i;
        vsidsPos[vsidsHeap[largest]] = largest;
        i = largest;
    }
}

// Neue Variable in den Heap einfügen
void Heuristic::heapInsert(int v) {
    vsidsHeap.push_back(v);
    int i = static_cast<int>(vsidsHeap.size()) - 1;
    vsidsPos[v] = i;
    heapifyUp(i);
}

// Falls Aktivität erhöht → nach oben korrigieren
void Heuristic::heapIncreaseKey(int v) {
    int i = vsidsPos[v];
    if (i >= 0) heapifyUp(i);
}