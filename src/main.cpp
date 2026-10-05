// chaosctl - CLI for the Network Latency & Packet-Loss Chaos Emulator (C++17). Run with sudo.
#include "emulator.hpp"
#include "experiments.hpp"
#include "measure.hpp"
#include "profiles.hpp"
#include "scheduler.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h>

namespace fs = std::filesystem;

static void usage() {
    std::cout <<
R"(Network Latency & Packet-Loss Chaos Emulator (run with sudo)

  chaosctl setup | teardown              create / remove the namespace lab
  chaosctl clear | show | list-profiles
  chaosctl profile <name>                apply a named profile (both directions)
  chaosctl apply [--delay ms] [--jitter ms] [--loss %] [--loss-corr %] [--duplicate %]
                 [--corrupt %] [--reorder %] [--rate kbit] [--ge P R]
  chaosctl random   [--duration 60] [--interval 5] [--max-delay 200] [--max-loss 20] [--seed N] [--log file]
  chaosctl flap     [--duration 60] [--up 8] [--down 3] [--log file]
  chaosctl timeline <scenario.json> [--log file]
  chaosctl measure  [--profile clean] [--out results/measure.csv]
  chaosctl experiment <latency loss tcp_loss tcp_rtt profiles burst chaos resilience | all>
                 [--reps 3] [--quick] [--scenario scenario.json]
  chaosctl monitor  [--duration 30]      live RTT monitor in the terminal
  chaosctl plot                          run plot_results.py (results/*.csv -> plots/*.png)
)";
}

struct Args {
    std::vector<std::string> pos;
    std::map<std::string, std::vector<std::string>> opt;
    bool has(const std::string& k) const { return opt.count(k) > 0; }
    std::string str(const std::string& k, const std::string& d) const { return has(k) ? opt.at(k)[0] : d; }
    double num(const std::string& k, double d) const {
        if (!has(k)) return d;
        try { return std::stod(opt.at(k)[0]); }
        catch (...) { throw std::runtime_error("--" + k + " needs a number, got '" + opt.at(k)[0] + "'"); }
    }
};

static Args parseArgs(int argc, char** argv, int start) {
    static const std::set<std::string> flags = {"quick"};
    static const std::map<std::string, int> arity = {{"ge", 2}};
    Args a;
    for (int i = start; i < argc; ++i) {
        std::string t = argv[i];
        if (t.rfind("--", 0) != 0) { a.pos.push_back(t); continue; }
        std::string k = t.substr(2);
        int n = flags.count(k) ? 0 : (arity.count(k) ? arity.at(k) : 1);
        if (n > 0 && i + n > argc - 1) throw std::runtime_error("--" + k + " needs " + std::to_string(n) + " value(s)");
        std::vector<std::string> vals;
        for (int j = 0; j < n; ++j) vals.push_back(argv[++i]);
        a.opt[k] = vals;
    }
    return a;
}

static std::string findScript(const std::string& name) {
    std::error_code ec;
    std::vector<fs::path> dirs;
    if (const char* h = std::getenv("CHAOS_HOME")) dirs.push_back(fs::path(h) / "scripts");
    fs::path exe = fs::read_symlink("/proc/self/exe", ec).parent_path();
    dirs.push_back(exe / ".." / "scripts");
    dirs.push_back(exe / "scripts");
    dirs.push_back(fs::path("scripts"));
    dirs.push_back(fs::path("."));
    for (auto& d : dirs) if (fs::exists(d / name, ec)) return fs::weakly_canonical(d / name, ec).string();
    throw std::runtime_error("cannot find " + name + " (set CHAOS_HOME to the project folder)");
}

static void printShow(NetemEmulator& emu) {
    for (auto& kv : emu.show()) std::cout << kv.first << ": " << kv.second << "\n";
}

static int run(const std::string& cmd, const Args& a) {
    if (cmd == "plot") {
        int rc = runInherit({"python3", "plot_results.py"});
        if (rc != 0) std::cerr << "plot failed: needs python3 + pandas + matplotlib (pip install -r requirements.txt) "
                                  "and plot_results.py in the current folder\n";
        return rc;
    }
    if (cmd == "list-profiles") {
        for (auto& p : profiles()) {
            std::string s = p.second.toNetem();
            std::cout << std::left << std::setw(10) << p.first << " " << (s.empty() ? "(none)" : s) << "\n";
        }
        return 0;
    }
    if (geteuid() != 0) throw std::runtime_error("Please run with sudo.");
    fs::create_directories("results");

    if (cmd == "setup" || cmd == "teardown") return runInherit({findScript(cmd + "_lab.sh")});

    NetemEmulator emu;
    if (cmd == "clear") { emu.clear(); std::cout << "cleared\n"; }
    else if (cmd == "show") printShow(emu);
    else if (cmd == "apply") {
        Impairment imp;
        imp.delay_ms = a.num("delay", 0); imp.jitter_ms = a.num("jitter", 0); imp.loss_pct = a.num("loss", 0);
        imp.loss_corr_pct = a.num("loss-corr", 0); imp.duplicate_pct = a.num("duplicate", 0);
        imp.corrupt_pct = a.num("corrupt", 0); imp.reorder_pct = a.num("reorder", 0);
        imp.rate_kbit = (int)a.num("rate", 0);
        if (a.has("ge")) { imp.ge_p = std::stod(a.opt.at("ge")[0]); imp.ge_r = std::stod(a.opt.at("ge")[1]); }
        imp.toNetem();  // validate before touching the network
        emu.apply(imp); printShow(emu);
    } else if (cmd == "profile") {
        if (a.pos.empty() || !hasProfile(a.pos[0])) throw std::runtime_error("usage: chaosctl profile <name>  (see list-profiles)");
        emu.apply(getProfile(a.pos[0]));
        std::cout << "applied " << a.pos[0] << ":\n"; printShow(emu);
    } else if (cmd == "random") {
        Guard g(emu);
        EventLogger lg(a.str("log", "results/events.csv"));
        std::optional<unsigned> seed;
        if (a.has("seed")) seed = (unsigned)a.num("seed", 0);
        randomChaos(emu, a.num("duration", 60), a.num("interval", 5), a.num("max-delay", 200), 50,
                    a.num("max-loss", 20), seed, &lg);
    } else if (cmd == "flap") {
        Guard g(emu);
        EventLogger lg(a.str("log", "results/events.csv"));
        flapping(emu, a.num("duration", 60), a.num("up", 8), a.num("down", 3), &lg);
    } else if (cmd == "timeline") {
        if (a.pos.empty()) throw std::runtime_error("usage: chaosctl timeline <scenario.json>");
        auto steps = loadScenario(a.pos[0]);
        Guard g(emu);
        EventLogger lg(a.str("log", "results/events.csv"));
        timeline(emu, steps, &lg);
    } else if (cmd == "measure") {
        std::string prof = a.str("profile", "clean");
        if (!hasProfile(prof)) throw std::runtime_error("unknown profile: " + prof);
        ensureServers();
        Guard g(emu);
        emu.apply(getProfile(prof)); sleepFor(0.5);
        Row row; row.add("profile", prof);
        pingStats(SERVER_IP, 30).addTo(row);
        row.add("tcp_mbps", tcpThroughput(SERVER_IP, 8)).add("http_s", httpTime());
        UdpStats u = udpStats(SERVER_IP, "5M", 5);
        row.add("udp_loss", u.loss).add("udp_jitter_ms", u.jitter_ms).add("udp_mbps", u.mbps);
        appendRow(a.str("out", "results/measure.csv"), row);
        for (auto& kv : row.f) std::cout << kv.first << "=" << (kv.second.empty() ? "None" : kv.second) << "  ";
        std::cout << "\n";
    } else if (cmd == "experiment") {
        if (a.pos.empty()) throw std::runtime_error("usage: chaosctl experiment <names...|all>");
        runExperiments(a.pos, (int)a.num("reps", 3), a.has("quick"), a.str("scenario", "scenario.json"));
    } else if (cmd == "monitor") {
        ensureServers();
        SignalScope sc;
        double secs = a.num("duration", 30), t0 = nowSec();
        int n = 0, lost = 0; double sum = 0;
        std::cout << "[time]  [rtt]      (Ctrl+C to stop)\n";
        while (nowSec() - t0 < secs) {
            OptD rtt = pingOnce(); ++n;
            std::cout << std::fixed << std::setprecision(1) << std::setw(6) << nowSec() - t0 << "s  ";
            if (rtt) { sum += *rtt; std::cout << *rtt << " ms"; } else { ++lost; std::cout << "LOST"; }
            std::cout << "   | samples " << n << " | loss " << 100.0 * lost / n << "% | avg "
                      << (n > lost ? sum / (n - lost) : 0.0) << " ms\n" << std::flush;
            sleepFor(0.3);
        }
    } else {
        usage(); return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") { usage(); return argc < 2 ? 1 : 0; }
    try {
        return run(argv[1], parseArgs(argc, argv, 2));
    } catch (const Interrupted&) {
        std::cerr << "\n[interrupted] network restored to clean state\n";
        return 130;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
