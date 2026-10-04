#include "experiments.hpp"
#include "measure.hpp"
#include "profiles.hpp"
#include "scheduler.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <fstream>

namespace fs = std::filesystem;
static const std::string RES = "results";
static const double MSS = 1448;  // bytes

const std::vector<std::string> EXPERIMENT_NAMES = {"latency", "loss", "tcp_loss", "tcp_rtt",
                                                   "profiles", "burst", "chaos", "resilience"};

using Fn = void (*)(NetemEmulator&, int, bool, const std::string&);

static std::string fresh(const std::string& name) {
    fs::create_directories(RES);
    std::string p = RES + "/" + name + ".csv";
    std::error_code ec; fs::remove(p, ec);
    return p;
}

static Impairment mk(double delay = 0, double loss = 0) {
    Impairment i; i.delay_ms = delay; i.loss_pct = loss; return i;
}

// Mathis et al. 1997: BW ~ (MSS/RTT) * 1.22/sqrt(p)
static OptD mathis(double rtt_s, double loss_pct) {
    if (loss_pct <= 0) return std::nullopt;
    return (MSS * 8 / rtt_s) * (1.22 / std::sqrt(loss_pct / 100)) / 1e6;
}

static void expLatency(NetemEmulator& emu, int reps, bool quick, const std::string&) {
    std::string out = fresh("latency");
    for (int d : {0, 25, 50, 100, 200, 400}) {
        emu.apply(mk(d)); sleepFor(0.5);
        for (int r = 0; r < reps; ++r) {
            PingStats st = pingStats(SERVER_IP, quick ? 10 : 30);
            Row row; row.add("oneway_delay_ms", d).add("expected_rtt_ms", 2 * d).add("rep", r);
            st.addTo(row); appendRow(out, row);
        }
    }
    emu.clear();
}

static void expLoss(NetemEmulator& emu, int reps, bool quick, const std::string&) {
    std::string out = fresh("loss");
    for (double p : {0.0, 1.0, 2.0, 5.0, 10.0, 20.0}) {
        emu.apply(mk(0, p)); sleepFor(0.5);
        for (int r = 0; r < reps; ++r) {
            PingStats st = pingStats(SERVER_IP, quick ? 40 : 100);
            double expected = std::round((1 - std::pow(1 - p / 100, 2)) * 100 * 100) / 100;
            Row row; row.add("loss_pct_per_dir", p).add("expected_roundtrip_loss", expected).add("rep", r);
            st.addTo(row); appendRow(out, row);
        }
    }
    emu.clear();
}

static void expTcpLoss(NetemEmulator& emu, int reps, bool quick, const std::string&) {
    std::string out = fresh("tcp_loss");
    const double delay = 25;   // loss on the data direction only, 50 ms RTT
    for (double p : {0.0, 0.1, 0.5, 1.0, 2.0, 5.0, 10.0}) {
        emu.applyPair(mk(delay, p), mk(delay)); sleepFor(0.5);
        for (int r = 0; r < reps; ++r) {
            OptD mb = tcpThroughput(SERVER_IP, quick ? 4 : 10);
            Row row; row.add("loss_pct", p).add("rep", r).add("tcp_mbps", mb)
                        .add("mathis_mbps", mathis(2 * delay / 1000 + 0.0005, p));
            appendRow(out, row);
        }
    }
    emu.clear();
}

static void expTcpRtt(NetemEmulator& emu, int reps, bool quick, const std::string&) {
    std::string out = fresh("tcp_rtt");
    const double p = 0.5;
    for (double d : {0.0, 10.0, 25.0, 50.0, 100.0, 200.0}) {
        emu.applyPair(mk(d, p), mk(d)); sleepFor(0.5);
        for (int r = 0; r < reps; ++r) {
            OptD mb = tcpThroughput(SERVER_IP, quick ? 4 : 10);
            Row row; row.add("rtt_ms", 2 * d).add("rep", r).add("tcp_mbps", mb)
                        .add("mathis_mbps", mathis(std::max(2 * d / 1000, 0.0005), p));
            appendRow(out, row);
        }
    }
    emu.clear();
}

static void expProfiles(NetemEmulator& emu, int reps, bool quick, const std::string&) {
    std::string out = fresh("profiles");
    for (auto& kv : profiles()) {
        if (kv.first == "outage") continue;
        emu.apply(kv.second); sleepFor(0.5);
        for (int r = 0; r < reps; ++r) {
            PingStats st = pingStats(SERVER_IP, quick ? 15 : 30);
            OptD mb = tcpThroughput(SERVER_IP, quick ? 4 : 8);
            OptD web = httpTime(HTTP_URL, quick ? 30 : 60);
            Row row; row.add("profile", kv.first).add("rep", r).add("tcp_mbps", mb).add("http_s", web);
            st.addTo(row); appendRow(out, row);
        }
    }
    emu.clear();
}

// Same average loss: random vs bursty (Gilbert model, mean burst = 4 packets)
static void expBurst(NetemEmulator& emu, int reps, bool quick, const std::string&) {
    std::string out = fresh("burst");
    const double R = 25.0;
    for (double avg : {1.0, 2.0, 5.0}) {
        double p_ge = avg * R / (100 - avg);
        Impairment burst; burst.ge_p = std::round(p_ge * 1e4) / 1e4; burst.ge_r = R; burst.ge_1h = 100; burst.ge_1k = 0;
        std::vector<std::pair<std::string, Impairment>> models = {{"random", mk(0, avg)}, {"burst", burst}};
        for (auto& m : models) {
            Impairment imp = m.second; imp.delay_ms = 25;
            emu.applyPair(imp, mk(25)); sleepFor(0.5);
            for (int r = 0; r < reps; ++r) {
                OptD mb = tcpThroughput(SERVER_IP, quick ? 4 : 10);
                Row row; row.add("avg_loss_pct", avg).add("model", m.first).add("rep", r).add("tcp_mbps", mb);
                appendRow(out, row);
            }
        }
    }
    emu.clear();
}

// Time series: continuous RTT + per-second TCP throughput while the scenario runs.
static void expChaos(NetemEmulator& emu, int, bool quick, const std::string& scenario) {
    for (const char* f : {"chaos_ping", "chaos_iperf", "chaos_events"}) fresh(f);
    std::vector<Step> steps = loadScenario(scenario);
    if (quick) for (auto& s : steps) s.seconds = std::min(s.seconds, 6.0);
    double total = 0; for (auto& s : steps) total += s.seconds;
    EventLogger logger(RES + "/chaos_events.csv");
    PingMonitor mon;
    std::vector<std::pair<double, double>> iperf_result;
    double t_start = nowSec();
    std::thread th([&] { iperf_result = tcpIntervals(SERVER_IP, (int)total); });
    mon.start();
    try {
        timeline(emu, steps, &logger);
    } catch (...) { mon.stop(); th.join(); throw; }
    th.join(); mon.stop();
    mon.save(RES + "/chaos_ping.csv");
    std::ofstream f(RES + "/chaos_iperf.csv");
    f << "timestamp,tcp_mbps\n" << std::fixed << std::setprecision(3);
    for (auto& p : iperf_result) f << t_start + p.first << "," << p.second << "\n";
}

// Naive client (one long attempt) vs resilient client (short timeout + retries), 6 s budget.
static void expResilience(NetemEmulator& emu, int, bool quick, const std::string&) {
    std::string out = fresh("resilience");
    const double budget = 6;
    const int trials = quick ? 4 : 15;
    for (const char* name : {"clean", "4g", "bad_wifi", "disaster"}) {
        emu.apply(getProfile(name)); sleepFor(0.5);
        for (int i = 0; i < trials; ++i) {
            checkStop();
            auto n = httpGet(SMALL_URL, budget);
            Row a; a.add("profile", name).add("client", "naive").add("trial", i).add("ok", n.first ? 1 : 0).add("seconds", n.second);
            appendRow(out, a);
            double t0 = nowSec(); bool ok = false;
            while (nowSec() - t0 < budget && !ok) {
                checkStop();
                double left = budget - (nowSec() - t0);
                ok = httpGet(SMALL_URL, std::min(1.5, left), 0.7).first;
            }
            Row b; b.add("profile", name).add("client", "resilient").add("trial", i).add("ok", ok ? 1 : 0).add("seconds", nowSec() - t0);
            appendRow(out, b);
        }
    }
    emu.clear();
}

void runExperiments(const std::vector<std::string>& names_in, int reps, bool quick, const std::string& scenario) {
    const std::vector<std::pair<std::string, Fn>> table = {
        {"latency", expLatency}, {"loss", expLoss}, {"tcp_loss", expTcpLoss}, {"tcp_rtt", expTcpRtt},
        {"profiles", expProfiles}, {"burst", expBurst}, {"chaos", expChaos}, {"resilience", expResilience}};
    std::vector<std::string> names = (names_in.size() == 1 && names_in[0] == "all") ? EXPERIMENT_NAMES : names_in;
    for (auto& n : names) {
        bool known = std::any_of(table.begin(), table.end(), [&](auto& t) { return t.first == n; });
        if (!known) throw std::runtime_error("unknown experiment: " + n);
    }
    ensureServers();
    NetemEmulator emu;
    Guard guard(emu);
    for (auto& n : names) {
        std::cout << "[experiment] " << n << " ...\n" << std::flush;
        for (auto& t : table) if (t.first == n) t.second(emu, reps, quick, scenario);
    }
    std::cout << "Done. CSVs in " << RES << "/\n";
}
