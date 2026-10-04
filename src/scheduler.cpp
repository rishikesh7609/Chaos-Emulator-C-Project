#include "scheduler.hpp"
#include "json.hpp"
#include "profiles.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

EventLogger::EventLogger(std::string path) : path_(std::move(path)) {
    if (!path_.empty()) {
        std::ofstream f(path_, std::ios::trunc);
        if (!f) throw std::runtime_error("cannot write " + path_ + " (does the folder exist?)");
        f << "timestamp,label,netem\n";
    }
}

void EventLogger::log(const std::string& label, const Impairment& imp) {
    double ts = nowSec();
    std::string spec = imp.toNetem();
    if (spec.empty()) spec = "clean";
    events.push_back({ts, label, spec});
    if (!path_.empty()) {
        std::ofstream f(path_, std::ios::app);
        f << std::fixed << std::setprecision(3) << ts << "," << label << "," << spec << "\n";
    }
}

namespace {
struct Cleanup {   // always: clear network + log "end"
    NetemEmulator& emu;
    EventLogger* logger;
    ~Cleanup() {
        try { emu.clear(); } catch (...) {}
        try { if (logger) logger->log("end", Impairment()); } catch (...) {}
    }
};
}  // namespace

void randomChaos(NetemEmulator& emu, double duration, double interval, double max_delay, double max_jitter,
                 double max_loss, std::optional<unsigned> seed, EventLogger* logger) {
    std::mt19937_64 rng(seed ? *seed : std::random_device{}());
    auto uni = [&](double hi) { return std::uniform_real_distribution<double>(0.0, hi)(rng); };
    auto rnd = [](double v, int dec) { double m = std::pow(10.0, dec); return std::round(v * m) / m; };
    double end = nowSec() + duration;
    Cleanup c{emu, logger};
    while (nowSec() < end) {
        Impairment imp;
        imp.delay_ms = rnd(uni(max_delay), 1);
        imp.jitter_ms = rnd(uni(max_jitter), 1);
        imp.loss_pct = rnd(uni(max_loss), 2);
        emu.apply(imp);
        if (logger) logger->log("random", imp);
        sleepFor(std::min(interval, std::max(0.0, end - nowSec())));
    }
}

void timeline(NetemEmulator& emu, const std::vector<Step>& steps, EventLogger* logger) {
    Cleanup c{emu, logger};
    for (auto& s : steps) {
        emu.apply(s.imp);
        if (logger) logger->log(s.label, s.imp);
        sleepFor(s.seconds);
    }
}

void flapping(NetemEmulator& emu, double duration, double up_s, double down_s, EventLogger* logger) {
    Impairment good, down = getProfile("outage");
    double end = nowSec() + duration;
    Cleanup c{emu, logger};
    while (nowSec() < end) {
        emu.apply(good);
        if (logger) logger->log("up", good);
        sleepFor(up_s);
        if (nowSec() >= end) break;
        emu.apply(down);
        if (logger) logger->log("down", down);
        sleepFor(down_s);
    }
}

std::vector<Step> loadScenario(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open scenario file: " + path);
    std::stringstream ss; ss << f.rdbuf();
    Json j = parseJson(ss.str());
    if (j.type != Json::Arr) throw std::runtime_error("scenario must be a JSON array of steps");
    std::vector<Step> steps;
    for (auto& s : j.arr) {
        const Json* d = s.get("duration");
        if (!d || d->type != Json::Num) throw std::runtime_error("scenario step missing numeric \"duration\"");
        if (const Json* p = s.get("profile")) {
            if (!hasProfile(p->s)) throw std::runtime_error("scenario: unknown profile \"" + p->s + "\"");
            steps.push_back({d->n, p->s, getProfile(p->s)});
        } else if (const Json* c = s.get("custom")) {
            Impairment imp;
            for (size_t i = 0; i < c->keys.size(); ++i) imp.set(c->keys[i], c->vals[i].n);
            steps.push_back({d->n, "custom", imp});
        } else {
            throw std::runtime_error("scenario step needs \"profile\" or \"custom\"");
        }
    }
    return steps;
}
