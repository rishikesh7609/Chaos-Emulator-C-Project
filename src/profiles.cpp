#include "profiles.hpp"
#include <stdexcept>

const ProfileList& profiles() {
    static const ProfileList P = {
        {"clean",     Impairment()},
        {"broadband", makeImpairment({{"delay_ms", 10}, {"jitter_ms", 2}})},
        {"4g",        makeImpairment({{"delay_ms", 25}, {"jitter_ms", 8}, {"loss_pct", 0.5}, {"rate_kbit", 20000}})},
        {"3g",        makeImpairment({{"delay_ms", 60}, {"jitter_ms", 20}, {"loss_pct", 1}, {"rate_kbit", 1500}})},
        {"bad_wifi",  makeImpairment({{"delay_ms", 15}, {"jitter_ms", 25}, {"loss_pct", 6}, {"loss_corr_pct", 30}, {"reorder_pct", 5}})},
        {"satellite", makeImpairment({{"delay_ms", 300}, {"jitter_ms", 20}, {"loss_pct", 1}, {"rate_kbit", 5000}})},
        {"disaster",  makeImpairment({{"delay_ms", 250}, {"jitter_ms", 100}, {"loss_pct", 20}, {"duplicate_pct", 3},
                                      {"corrupt_pct", 0.5}, {"rate_kbit", 256}})},
        {"outage",    makeImpairment({{"loss_pct", 100}})},
    };
    return P;
}

bool hasProfile(const std::string& name) {
    for (auto& p : profiles()) if (p.first == name) return true;
    return false;
}

const Impairment& getProfile(const std::string& name) {
    for (auto& p : profiles()) if (p.first == name) return p.second;
    throw std::out_of_range("unknown profile: " + name);
}
