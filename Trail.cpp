#include "Trail.h"
#include <iostream>

void Trail::assign(const Literal& lit, int level, int reason_idx) {
    trail.push_back({lit, level, reason_idx});
}

bool Trail::isAssigned(int var) const {
    for (const auto& e : trail) if (e.lit.getVar() == var) return true;
    return false;
}

int Trail::currentLevel() const {
    return trail.empty() ? 0 : trail.back().level;
}

Literal& Trail::getLastLiteral() {
    return trail.back().lit;
}

int Trail::getLevelOfVar(int var) const {
    for (const auto& e : trail) if (e.lit.getVar() == var) return e.level;
    return 0; // unassigned → Level 0
}

int Trail::getReasonIndexOfVar(int var) const {
    for (auto it = trail.rbegin(); it != trail.rend(); ++it) {
        if (it->lit.getVar() == var) return it->reason_idx;
    }
    return -1; // Variable noch nicht zugewiesen
}

std::vector<int> Trail::popAboveLevel(int level) {
    std::vector<int> popped;
    while (!trail.empty() && trail.back().level > level) {
        popped.push_back(trail.back().lit.getVar());
        trail.pop_back();
    }
    return popped;
}

const std::vector<Trail::TrailEntry>& Trail::getTrail() const {
    return trail;
}

std::ostream& operator<<(std::ostream& os, const Trail& t) {
    os << "{ ";
    for (size_t i = 0; i < t.trail.size(); ++i) {
        const auto& e = t.trail[i];
        os << e.lit << "[L" << e.level << ", R:";
        if (e.reason_idx >= 0) os << e.reason_idx; else os << "decision";
        os << "]";
        if (i + 1 < t.trail.size()) os << ", ";
    }
    os << " }";
    return os;
}