#ifndef UTIL_HPP
#define UTIL_HPP
#include <csignal>
#include <stdexcept>
#include <string>
#include <vector>

struct ProcResult {
    int code = -1;
    bool timed_out = false;
    std::string out, err;
};

struct Interrupted : std::runtime_error {
    Interrupted() : std::runtime_error("interrupted") {}
};

// Runs argv directly (NO shell -> no injection). Captures stdout/stderr. timeout_s<=0 = none.
ProcResult runProcess(const std::vector<std::string>& argv, double timeout_s = 0);
int runInherit(const std::vector<std::string>& argv);          // inherits terminal, returns exit code
void spawnDetached(const std::vector<std::string>& argv);      // background daemon, no output

std::vector<std::string> splitWs(const std::string& s);
std::string joinStr(const std::vector<std::string>& v, const std::string& sep);
std::string trim(const std::string& s);
std::string fmtG(double v);      // like Python's format(v, "g")
double nowSec();                 // seconds since epoch

// Ctrl+C / SIGTERM handling: inside a SignalScope the signals only set a flag;
// sleepFor()/checkStop() then throw Interrupted so RAII cleanup can run.
class SignalScope {
public:
    SignalScope();
    ~SignalScope();
    SignalScope(const SignalScope&) = delete;
    SignalScope& operator=(const SignalScope&) = delete;
private:
    struct sigaction old_int_, old_term_;
};
bool stopRequested();
void checkStop();
void sleepFor(double secs);      // interruptible sleep (main thread)

#endif
