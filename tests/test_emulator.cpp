// Pure unit tests (no root, no lab needed). Integration is covered by `chaosctl experiment latency loss --quick`.
#include "emulator.hpp"
#include "json.hpp"
#include "profiles.hpp"
#include "scheduler.hpp"
#include <fstream>
#include <iostream>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAILED: " #cond " (line " << __LINE__ << ")\n"; ++failures; } } while (0)
#define PASS(name) std::cout << "PASSED: " name "\n"

static bool contains(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }

static void testCleanIsEmpty() { CHECK(Impairment().toNetem().empty()); CHECK(Impairment().isClean()); PASS("clean_is_empty"); }

static void testDelayJitterLoss() {
    Impairment i = makeImpairment({{"delay_ms", 100}, {"jitter_ms", 20}, {"loss_pct", 5}});
    std::string s = i.toNetem();
    CHECK(contains(s, "delay 100ms 20ms distribution normal")); CHECK(contains(s, "loss 5%"));
    CHECK(s.rfind("limit 10000 ", 0) == 0);
    PASS("delay_jitter_loss_spec");
}

static void testReorderAddsDelay() { CHECK(contains(makeImpairment({{"reorder_pct", 5}}).toNetem(), "delay 10ms")); PASS("reorder_adds_delay"); }

static void testGemodel() {
    CHECK(contains(makeImpairment({{"ge_p", 0.5}, {"ge_r", 25}}).toNetem(), "loss gemodel 0.5% 25% 100% 0%"));
    PASS("gemodel_spec");
}

static void testGemodelRequiresR() {
    bool threw = false;
    try { makeImpairment({{"ge_p", 1}}).toNetem(); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw); PASS("gemodel_requires_r");
}

static void testFullSpec() {
    CHECK(getProfile("disaster").toNetem() ==
          "limit 10000 delay 250ms 100ms distribution normal loss 20% duplicate 3% corrupt 0.5% rate 256kbit");
    CHECK(getProfile("bad_wifi").toNetem() ==
          "limit 10000 delay 15ms 25ms distribution normal loss 6% 30% reorder 5% 50%");
    PASS("full_profile_specs");
}

static void testProfiles() {
    CHECK(profiles().size() == 8); CHECK(hasProfile("outage")); CHECK(!hasProfile("nope"));
    CHECK(getProfile("clean").isClean()); CHECK(getProfile("outage").toNetem() == "limit 10000 loss 100%");
    PASS("profiles");
}

static void testJsonAndScenario() {
    Json j = parseJson(R"({"a":[1,2.5,-3e2],"b":{"c":"x\ny"},"d":true,"e":null})");
    CHECK(j.get("a")->arr.size() == 3); CHECK(j.get("a")->arr[2].n == -300); CHECK(j.get("b")->get("c")->s == "x\ny");
    const std::string path = "/tmp/_chaos_test_scenario.json";
    { std::ofstream f(path); f << R"([{"duration":20,"profile":"4g"},{"duration":10,"custom":{"delay_ms":100,"loss_pct":5}}])"; }
    auto steps = loadScenario(path);
    CHECK(steps.size() == 2); CHECK(steps[0].label == "4g"); CHECK(steps[0].seconds == 20);
    CHECK(steps[1].label == "custom"); CHECK(steps[1].imp.delay_ms == 100); CHECK(steps[1].imp.loss_pct == 5);
    PASS("json_and_scenario");
}

int main() {
    std::cout << "Running C++ unit tests...\n";
    testCleanIsEmpty(); testDelayJitterLoss(); testReorderAddsDelay(); testGemodel(); testGemodelRequiresR();
    testFullSpec(); testProfiles(); testJsonAndScenario();
    if (failures) { std::cerr << failures << " check(s) FAILED\n"; return 1; }
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
