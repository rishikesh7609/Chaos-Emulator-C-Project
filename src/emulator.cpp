#include "emulator.hpp"
#include <stdexcept>
#include <unistd.h>

Impairment& Impairment::set(const std::string& k, double v) {
    if (k == "delay_ms") delay_ms = v;
    else if (k == "jitter_ms") jitter_ms = v;
    else if (k == "loss_pct") loss_pct = v;
    else if (k == "loss_corr_pct") loss_corr_pct = v;
    else if (k == "ge_p") ge_p = v;
    else if (k == "ge_r") ge_r = v;
    else if (k == "ge_1h") ge_1h = v;
    else if (k == "ge_1k") ge_1k = v;
    else if (k == "duplicate_pct") duplicate_pct = v;
    else if (k == "corrupt_pct") corrupt_pct = v;
    else if (k == "reorder_pct") reorder_pct = v;
    else if (k == "rate_kbit") rate_kbit = (int)v;
    else throw std::invalid_argument("unknown impairment field: " + k);
    return *this;
}

Impairment makeImpairment(std::initializer_list<std::pair<const char*, double>> kv) {
    Impairment i;
    for (auto& p : kv) i.set(p.first, p.second);
    return i;
}

std::string Impairment::toNetem() const {
    std::vector<std::string> parts;
    double delay = delay_ms;
    if (reorder_pct != 0 && delay == 0) delay = 10;   // netem can only reorder delayed packets
    if (delay != 0) {
        std::string s = "delay " + fmtG(delay) + "ms";
        if (jitter_ms != 0) s += " " + fmtG(jitter_ms) + "ms distribution normal";
        parts.push_back(s);
    }
    if (ge_p > 0) {
        if (ge_r <= 0) throw std::invalid_argument("ge_r must be > 0 when ge_p is set");
        parts.push_back("loss gemodel " + fmtG(ge_p) + "% " + fmtG(ge_r) + "% " + fmtG(ge_1h) + "% " + fmtG(ge_1k) + "%");
    } else if (loss_pct != 0) {
        std::string s = "loss " + fmtG(loss_pct) + "%";
        if (loss_corr_pct != 0) s += " " + fmtG(loss_corr_pct) + "%";
        parts.push_back(s);
    }
    if (duplicate_pct != 0) parts.push_back("duplicate " + fmtG(duplicate_pct) + "%");
    if (corrupt_pct != 0) parts.push_back("corrupt " + fmtG(corrupt_pct) + "%");
    if (reorder_pct != 0) parts.push_back("reorder " + fmtG(reorder_pct) + "% 50%");
    if (rate_kbit != 0) parts.push_back("rate " + std::to_string(rate_kbit) + "kbit");
    if (parts.empty()) return "";
    return "limit 10000 " + joinStr(parts, " ");
}

NetemEmulator::NetemEmulator(bool require_root) {
    ifaces_ = {{CLIENT_NS, CLIENT_DEV}, {SERVER_NS, SERVER_DEV}};
    if (require_root && geteuid() != 0)
        throw std::runtime_error("Root needed (tc / ip netns). Run with sudo.");
}

std::string NetemEmulator::run(const std::string& ns, const std::string& cmd) {
    std::vector<std::string> argv;
    if (!ns.empty()) argv = {"ip", "netns", "exec", ns};
    for (auto& w : splitWs(cmd)) argv.push_back(w);
    ProcResult r = runProcess(argv);
    if (r.code != 0) throw std::runtime_error(joinStr(argv, " ") + "\n" + trim(r.err));
    return r.out;
}

void NetemEmulator::setOne(const Iface& i, const Impairment& imp) {
    std::string spec = imp.toNetem();
    if (spec.empty()) {
        try { run(i.ns, "tc qdisc del dev " + i.dev + " root"); } catch (const std::runtime_error&) {}
    } else {
        run(i.ns, "tc qdisc replace dev " + i.dev + " root netem " + spec);
    }
}

void NetemEmulator::applyPair(const Impairment& c, const Impairment& s) {
    setOne(ifaces_[0], c);   // client -> server
    setOne(ifaces_[1], s);   // server -> client
}

void NetemEmulator::clear() {
    for (auto& i : ifaces_) {
        try { run(i.ns, "tc qdisc del dev " + i.dev + " root"); } catch (const std::runtime_error&) {}
    }
}

std::vector<std::pair<std::string, std::string>> NetemEmulator::show() {
    std::vector<std::pair<std::string, std::string>> out;
    for (auto& i : ifaces_)
        out.emplace_back(i.ns + "/" + i.dev, trim(run(i.ns, "tc qdisc show dev " + i.dev)));
    return out;
}
