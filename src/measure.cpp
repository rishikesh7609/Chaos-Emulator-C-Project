#include "measure.hpp"
#include "json.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <sys/stat.h>

static std::string fmtNum(double v) { std::ostringstream o; o << std::setprecision(10) << v; return o.str(); }
Row& Row::add(const std::string& k, double v) { return add(k, fmtNum(v)); }
Row& Row::add(const std::string& k, OptD v) { return add(k, v ? fmtNum(*v) : std::string()); }

void appendRow(const std::string& path, const Row& row) {
    struct stat st;
    bool is_new = stat(path.c_str(), &st) != 0;
    std::ofstream f(path, std::ios::app);
    if (!f) throw std::runtime_error("cannot write " + path);
    if (is_new) {
        for (size_t i = 0; i < row.f.size(); ++i) f << (i ? "," : "") << row.f[i].first;
        f << "\n";
    }
    for (size_t i = 0; i < row.f.size(); ++i) f << (i ? "," : "") << row.f[i].second;
    f << "\n";
}

void PingStats::addTo(Row& r) const {
    r.add("rtt_min", rtt_min).add("rtt_avg", rtt_avg).add("rtt_max", rtt_max).add("jitter", jitter).add("loss", loss);
}

static ProcResult nsRun(const std::string& ns, const std::vector<std::string>& args, double timeout = 0) {
    std::vector<std::string> a = {"ip", "netns", "exec", ns};
    a.insert(a.end(), args.begin(), args.end());
    return runProcess(a, timeout);
}

void ensureServers() {
    ProcResult r = nsRun(SERVER_NS, {"ss", "-ltn"});
    if (r.code != 0)
        throw std::runtime_error("lab not found (namespace 'server' missing). Run: sudo ./chaosctl setup");
    if (r.out.find(":5201") == std::string::npos)
        spawnDetached({"ip", "netns", "exec", SERVER_NS, "iperf3", "-s"});
    if (r.out.find(":8000") == std::string::npos)
        spawnDetached({"ip", "netns", "exec", SERVER_NS, "python3", "-m", "http.server", "8000",
                       "--directory", "/tmp/testfiles"});
    sleepFor(1);
}

PingStats pingStats(const std::string& target, int count, double interval) {
    ProcResult r = nsRun(CLIENT_NS, {"ping", "-c", std::to_string(count), "-i", fmtG(interval), "-W", "2", target},
                         count * (interval + 2) + 10);
    std::string out = r.timed_out ? "" : r.out;
    PingStats s;
    std::smatch m;
    if (std::regex_search(out, m, std::regex(R"(([0-9.]+)% packet loss)"))) s.loss = std::stod(m[1]);
    if (std::regex_search(out, m, std::regex(R"(= ([0-9.]+)/([0-9.]+)/([0-9.]+)/([0-9.]+))"))) {
        s.rtt_min = std::stod(m[1]); s.rtt_avg = std::stod(m[2]);
        s.rtt_max = std::stod(m[3]); s.jitter = std::stod(m[4]);
    }
    return s;
}

OptD pingOnce(const std::string& target, double timeout) {
    ProcResult r = nsRun(CLIENT_NS, {"ping", "-c", "1", "-W", fmtG(timeout), target}, timeout + 3);
    std::smatch m;
    if (!r.timed_out && std::regex_search(r.out, m, std::regex(R"(time=([0-9.]+) ms)"))) return std::stod(m[1]);
    return std::nullopt;
}

static bool parseIperf(const ProcResult& r, Json& j) {
    if (r.timed_out) return false;
    try { j = parseJson(r.out); } catch (...) { return false; }
    return j.type == Json::Obj;
}

OptD tcpThroughput(const std::string& target, int t) {
    ProcResult r = nsRun(CLIENT_NS, {"iperf3", "-c", target, "-t", std::to_string(t), "-J"}, t + 20);
    Json j;
    if (!parseIperf(r, j) || j.has("error")) return std::nullopt;
    const Json* b = jget(&j, {"end", "sum_received", "bits_per_second"});
    return b ? OptD(b->n / 1e6) : std::nullopt;
}

std::vector<std::pair<double, double>> tcpIntervals(const std::string& target, int t) {
    ProcResult r = nsRun(CLIENT_NS, {"iperf3", "-c", target, "-t", std::to_string(t), "-i", "1", "-J"}, t + 30);
    std::vector<std::pair<double, double>> out;
    Json j;
    if (!parseIperf(r, j)) return out;
    const Json* iv = j.get("intervals");
    if (!iv) return out;
    for (auto& e : iv->arr) {
        const Json* end = jget(&e, {"sum", "end"});
        const Json* bps = jget(&e, {"sum", "bits_per_second"});
        if (end && bps) out.emplace_back(end->n, bps->n / 1e6);
    }
    return out;
}

UdpStats udpStats(const std::string& target, const std::string& bitrate, int t) {
    ProcResult r = nsRun(CLIENT_NS, {"iperf3", "-c", target, "-u", "-b", bitrate, "-t", std::to_string(t), "-J"}, t + 20);
    UdpStats s;
    Json j;
    if (!parseIperf(r, j)) return s;
    const Json* sum = jget(&j, {"end", "sum"});
    if (!sum) return s;
    if (auto* v = sum->get("lost_percent")) s.loss = v->n;
    if (auto* v = sum->get("jitter_ms")) s.jitter_ms = v->n;
    if (auto* v = sum->get("bits_per_second")) s.mbps = v->n / 1e6;
    return s;
}

std::pair<bool, double> httpGet(const std::string& url, double max_time, double connect_timeout) {
    std::vector<std::string> a = {"curl", "-s", "-o", "/dev/null", "-w", "%{time_total}", "--max-time", fmtG(max_time)};
    if (connect_timeout > 0) { a.push_back("--connect-timeout"); a.push_back(fmtG(connect_timeout)); }
    a.push_back(url);
    ProcResult r = nsRun(CLIENT_NS, a, max_time + 5);
    if (r.timed_out) return {false, max_time};
    try { return {r.code == 0, std::stod(r.out)}; } catch (...) { return {false, max_time}; }
}

OptD httpTime(const std::string& url, double max_time) {
    auto p = httpGet(url, max_time);
    return p.first ? OptD(p.second) : std::nullopt;
}

void PingMonitor::start() {
    halt_ = false;
    th_ = std::thread([this] {
        while (!halt_) {
            double t0 = nowSec();
            OptD rtt = pingOnce();
            samples_.emplace_back(t0, rtt);
            double wait = period_ - (nowSec() - t0);
            while (wait > 0 && !halt_) {
                double s = std::min(wait, 0.05);
                std::this_thread::sleep_for(std::chrono::duration<double>(s));
                wait -= s;
            }
        }
    });
}
void PingMonitor::stop() {
    halt_ = true;
    if (th_.joinable()) th_.join();
}
void PingMonitor::save(const std::string& path) const {
    std::ofstream f(path);
    f << "timestamp,rtt_ms\n" << std::fixed << std::setprecision(3);
    for (auto& s : samples_) {
        f << s.first << ",";
        if (s.second) f << std::setprecision(3) << *s.second;
        f << "\n";
    }
}
