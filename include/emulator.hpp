#ifndef EMULATOR_HPP
#define EMULATOR_HPP
// Core engine: wraps Linux `tc netem` inside the client/server network-namespace lab.
#include "util.hpp"
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

inline constexpr const char* CLIENT_NS = "client";
inline constexpr const char* SERVER_NS = "server";
inline constexpr const char* CLIENT_DEV = "veth-c";
inline constexpr const char* SERVER_DEV = "veth-s";
inline constexpr const char* CLIENT_IP = "10.0.0.1";
inline constexpr const char* SERVER_IP = "10.0.0.2";

// Impairment applied to ONE direction (egress of one interface).
struct Impairment {
    double delay_ms = 0;
    double jitter_ms = 0;
    double loss_pct = 0;        // random (Bernoulli) loss
    double loss_corr_pct = 0;   // loss correlation
    double ge_p = 0;            // Gilbert-Elliott: P(good->bad) in %
    double ge_r = 0;            //   P(bad->good) in %  (mean burst = 100/ge_r packets)
    double ge_1h = 100;         //   loss prob in bad state in %
    double ge_1k = 0;           //   loss prob in good state in %
    double duplicate_pct = 0;
    double corrupt_pct = 0;
    double reorder_pct = 0;
    int rate_kbit = 0;          // 0 = unlimited

    bool isClean() const { return toNetem().empty(); }
    std::string toNetem() const;                       // throws std::invalid_argument on bad GE params
    Impairment& set(const std::string& key, double v); // key = same names as Python's dataclass
};

Impairment makeImpairment(std::initializer_list<std::pair<const char*, double>> kv);

class NetemEmulator {
public:
    explicit NetemEmulator(bool require_root = true);

    void applyPair(const Impairment& client_imp, const Impairment& server_imp);  // asymmetric
    void apply(const Impairment& imp) { applyPair(imp, imp); }                  // both directions
    void clear();
    std::vector<std::pair<std::string, std::string>> show();

private:
    struct Iface { std::string ns, dev; };
    std::vector<Iface> ifaces_;
    std::string run(const std::string& ns, const std::string& cmd);
    void setOne(const Iface& i, const Impairment& imp);
};

// RAII: always restores a clean network, even on Ctrl+C / SIGTERM / exception.
class Guard {
public:
    explicit Guard(NetemEmulator& e) : emu_(e) {}
    ~Guard() { try { emu_.clear(); } catch (...) {} }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
private:
    SignalScope scope_;   // declared first-in-use: handlers stay installed while clear() runs
    NetemEmulator& emu_;
};

#endif
