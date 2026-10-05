# Network Latency & Packet-Loss Chaos Emulator (C++17)

C++ port of the Python project. It injects latency, jitter, loss (random + bursty Gilbert-Elliott),
duplication, corruption, reordering and bandwidth limits using Linux `tc netem`, **inside an isolated
network-namespace lab** (never your real `eth0`). It runs chaos schedules, measures the impact
(ping / iperf3 / curl) and writes CSVs that `plot_results.py` turns into graphs.

```
[client ns 10.0.0.1] --veth-c==veth-s-- [server ns 10.0.0.2]
   ping / iperf3 / curl        netem on BOTH interfaces       iperf3 -s, http.server :8000
```

## Requirements
Linux (VM / native / WSL2 with `sch_netem`), root, and:
```bash
sudo apt install -y g++ cmake iproute2 iperf3 curl ethtool python3 python3-pip
pip install -r requirements.txt        # only needed for `chaosctl plot`
```

## Build
```bash
mkdir build && cd build && cmake .. && make      # -> build/chaosctl, build/test_emulator
ctest --output-on-failure                        # unit tests (no root needed)
# no CMake?  ./build.sh   (or: sh build.sh)
```
Run from the project folder (so `scenario.json`, `results/` and `plot_results.py` are found):

## Quick start
```bash
sudo ./build/chaosctl setup                        # create lab (runs scripts/setup_lab.sh)
./build/chaosctl list-profiles
sudo ./build/chaosctl profile 3g
sudo ip netns exec client ping -c 5 10.0.0.2       # observe
sudo ./build/chaosctl apply --delay 100 --jitter 20 --loss 5
sudo ./build/chaosctl apply --delay 20 --ge 0.5 25 # burst loss (Gilbert model)
sudo ./build/chaosctl show
sudo ./build/chaosctl clear
sudo ./build/chaosctl monitor --duration 30        # live RTT in the terminal
```

## Chaos modes
```bash
sudo ./build/chaosctl random   --duration 120 --interval 5 --seed 42
sudo ./build/chaosctl flap     --duration 60 --up 8 --down 3
sudo ./build/chaosctl timeline scenario.json
```
Every change is logged to `results/events.csv`. Ctrl+C / SIGTERM always restores a clean network.

## Experiments -> graphs
```bash
sudo ./build/chaosctl experiment all --quick       # ~5 min smoke test first
sudo ./build/chaosctl experiment all --reps 3      # real run (~40-60 min)
./build/chaosctl plot                              # results/*.csv -> plots/*.png
```
Experiments: `latency loss tcp_loss tcp_rtt profiles burst chaos resilience`.

## Cleanup
`sudo ./build/chaosctl teardown`

## Layout
| Path | Role (Python equivalent) |
|---|---|
| `include/emulator.hpp`, `src/emulator.cpp` | `Impairment`, `NetemEmulator`, RAII `Guard` (`emulator.py`) |
| `src/profiles.cpp` | 8 named profiles (`profiles.py`) |
| `src/scheduler.cpp` | random / flap / timeline + `EventLogger` + scenario loader (`scheduler.py`) |
| `src/measure.cpp` | ping, iperf3 TCP/UDP, curl, `PingMonitor` (`measure.py`) |
| `src/experiments.cpp` | the 8 experiments + Mathis model (`experiments.py`) |
| `src/main.cpp` | CLI (`chaosctl.py`) |
| `src/util.cpp`, `include/json.hpp` | shell-free process runner, signals, mini JSON parser |
| `scripts/*.sh` | lab setup / teardown (unchanged) |
| `plot_results.py` | plotting, unchanged (reads the same CSV columns) |
| `tests/test_emulator.cpp` | unit tests for netem strings, profiles, JSON, scenarios |

## Notes
- netem affects **egress only**; it is applied on both ends. `--delay 100` => RTT ~200 ms.
- No shell is ever used to run commands (`fork`/`execvp`), so arguments cannot inject commands.
- `random --seed N` is reproducible in C++, but the sequence differs from Python's (different RNG).
- The Streamlit dashboard (`dashboard.py`) is not ported; `chaosctl monitor` covers live RTT in the terminal.
