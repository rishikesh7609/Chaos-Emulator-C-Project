#ifndef MEASURE_HPP
#define MEASURE_HPP
// Measurement harness: ping (RTT/jitter/loss), iperf3 (TCP/UDP), curl (page load).
#include "emulator.hpp"
#include <atomic>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

inline const std::string HTTP_URL = std::string("http://") + SERVER_IP + ":8000/file";
inline const std::string SMALL_URL = std::string("http://") + SERVER_IP + ":8000/small";

using OptD = std::optional<double>;

// One CSV row (ordered columns). Empty optional -> empty cell (like Python's None).
struct Row {
    std::vector<std::pair<std::string, std::string>> f;
    Row& add(const std::string& k, const std::string& v) { f.emplace_back(k, v); return *this; }
    Row& add(const std::string& k, double v);
    Row& add(const std::string& k, OptD v);
};
void appendRow(const std::string& path, const Row& row);

struct PingStats {
    OptD rtt_min, rtt_avg, rtt_max, jitter;
    double loss = 100.0;
    void addTo(Row& r) const;
};
struct UdpStats { OptD loss, jitter_ms, mbps; };

void ensureServers();   // starts iperf3 + http.server inside the server namespace if needed
PingStats pingStats(const std::string& target = SERVER_IP, int count = 30, double interval = 0.2);
OptD pingOnce(const std::string& target = SERVER_IP, double timeout = 1);
OptD tcpThroughput(const std::string& target = SERVER_IP, int t = 10);
std::vector<std::pair<double, double>> tcpIntervals(const std::string& target = SERVER_IP, int t = 60);
UdpStats udpStats(const std::string& target = SERVER_IP, const std::string& bitrate = "5M", int t = 10);
std::pair<bool, double> httpGet(const std::string& url = HTTP_URL, double max_time = 60, double connect_timeout = 0);
OptD httpTime(const std::string& url = HTTP_URL, double max_time = 60);

// Background RTT sampler for time-series plots.
class PingMonitor {
public:
    explicit PingMonitor(double period = 0.5) : period_(period) {}
    ~PingMonitor() { stop(); }
    void start();
    void stop();
    void save(const std::string& path) const;
private:
    double period_;
    std::atomic<bool> halt_{false};
    std::thread th_;
    std::vector<std::pair<double, OptD>> samples_;
};

#endif
