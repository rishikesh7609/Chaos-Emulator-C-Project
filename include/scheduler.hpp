#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP
// Chaos modes: random walk, scripted timeline (JSON), flapping outage. All log every change.
#include "emulator.hpp"
#include <optional>
#include <string>
#include <tuple>
#include <vector>

struct Step { double seconds; std::string label; Impairment imp; };

struct Event { double timestamp; std::string label; std::string netem; };

class EventLogger {
public:
    explicit EventLogger(std::string path = "");
    void log(const std::string& label, const Impairment& imp);
    std::vector<Event> events;
private:
    std::string path_;
};

void randomChaos(NetemEmulator& emu, double duration, double interval = 5, double max_delay = 200,
                 double max_jitter = 50, double max_loss = 20, std::optional<unsigned> seed = std::nullopt,
                 EventLogger* logger = nullptr);
void timeline(NetemEmulator& emu, const std::vector<Step>& steps, EventLogger* logger = nullptr);
void flapping(NetemEmulator& emu, double duration, double up_s = 8, double down_s = 3, EventLogger* logger = nullptr);

// JSON: [{"duration": 20, "profile": "3g"}, {"duration": 10, "custom": {"delay_ms": 100, "loss_pct": 5}}]
std::vector<Step> loadScenario(const std::string& path);

#endif
