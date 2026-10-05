#include "util.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static void onSignal(int) { g_stop = 1; }

SignalScope::SignalScope() {
    g_stop = 0;
    struct sigaction sa {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // no SA_RESTART: blocking calls are interrupted
    sigaction(SIGINT, &sa, &old_int_);
    sigaction(SIGTERM, &sa, &old_term_);
}
SignalScope::~SignalScope() {
    sigaction(SIGINT, &old_int_, nullptr);
    sigaction(SIGTERM, &old_term_, nullptr);
}
bool stopRequested() { return g_stop != 0; }
void checkStop() { if (g_stop) throw Interrupted(); }

double nowSec() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

void sleepFor(double secs) {
    double end = nowSec() + secs;
    checkStop();
    while (true) {
        double left = end - nowSec();
        if (left <= 0) break;
        std::this_thread::sleep_for(std::chrono::duration<double>(std::min(left, 0.05)));
        checkStop();
    }
}

std::vector<std::string> splitWs(const std::string& s) {
    std::istringstream is(s);
    std::vector<std::string> v;
    std::string w;
    while (is >> w) v.push_back(w);
    return v;
}
std::string joinStr(const std::vector<std::string>& v, const std::string& sep) {
    std::string o;
    for (size_t i = 0; i < v.size(); ++i) { if (i) o += sep; o += v[i]; }
    return o;
}
std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
std::string fmtG(double v) { std::ostringstream o; o << v; return o.str(); }

static std::vector<char*> toArgv(const std::vector<std::string>& argv) {
    std::vector<char*> a;
    for (auto& s : argv) a.push_back(const_cast<char*>(s.c_str()));
    a.push_back(nullptr);
    return a;
}

ProcResult runProcess(const std::vector<std::string>& argv, double timeout_s) {
    ProcResult r;
    int po[2], pe[2];
    if (pipe(po) != 0 || pipe(pe) != 0) throw std::runtime_error("pipe() failed");
    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork() failed");
    if (pid == 0) {
        dup2(po[1], 1); dup2(pe[1], 2);
        close(po[0]); close(po[1]); close(pe[0]); close(pe[1]);
        int dn = open("/dev/null", O_RDONLY);
        if (dn >= 0) dup2(dn, 0);
        auto a = toArgv(argv);
        execvp(a[0], a.data());
        _exit(127);
    }
    close(po[1]); close(pe[1]);
    int fds[2] = {po[0], pe[0]};
    std::string* bufs[2] = {&r.out, &r.err};
    bool is_open[2] = {true, true};
    double deadline = timeout_s > 0 ? nowSec() + timeout_s : 0;
    char buf[4096];
    while (is_open[0] || is_open[1]) {
        pollfd p[2]; int idx[2]; int n = 0;
        for (int i = 0; i < 2; ++i) if (is_open[i]) { p[n] = {fds[i], POLLIN, 0}; idx[n++] = i; }
        int wait_ms = -1;
        if (deadline > 0) {
            double left = deadline - nowSec();
            if (left <= 0) { r.timed_out = true; kill(pid, SIGKILL); break; }
            wait_ms = (int)(left * 1000) + 1;
        }
        int pr = poll(p, n, wait_ms);
        if (pr < 0) { if (errno == EINTR) continue; break; }
        for (int k = 0; k < n; ++k) {
            if (!p[k].revents) continue;
            ssize_t m = read(p[k].fd, buf, sizeof buf);
            if (m > 0) bufs[idx[k]]->append(buf, (size_t)m);
            else { close(p[k].fd); is_open[idx[k]] = false; }
        }
    }
    for (int i = 0; i < 2; ++i) if (is_open[i]) close(fds[i]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    r.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return r;
}

int runInherit(const std::vector<std::string>& argv) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        auto a = toArgv(argv);
        execvp(a[0], a.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

void spawnDetached(const std::vector<std::string>& argv) {
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        if (fork() > 0) _exit(0);  // double fork: no zombies
        setsid();
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); }
        auto a = toArgv(argv);
        execvp(a[0], a.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
}
