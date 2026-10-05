#ifndef EXPERIMENTS_HPP
#define EXPERIMENTS_HPP
// All experiments. Each writes results/<name>.csv (same columns as the Python version),
// so plot_results.py can turn them into graphs.
#include <string>
#include <vector>

extern const std::vector<std::string> EXPERIMENT_NAMES;
void runExperiments(const std::vector<std::string>& names, int reps, bool quick,
                    const std::string& scenario = "scenario.json");

#endif
