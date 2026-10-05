# Network Latency & Packet-Loss Chaos Emulator

A C++17 based Linux network chaos emulator for testing how applications behave under unreliable network conditions such as latency, jitter, packet loss, bandwidth limitations, packet reordering, duplication, corruption, and network outages.

The project creates an isolated virtual network using Linux network namespaces and virtual Ethernet (veth) interfaces. Linux Traffic Control (`tc`) and `netem` are then used to introduce controlled network impairments.

The main goal is to reproduce real-world network problems in a safe and repeatable environment without affecting the host's actual network connection.

---

## 📌 Why This Project?

Applications are usually developed and tested under ideal network conditions.

For example:

```text
Application
     ↓
Localhost
     ↓
Very low latency
     ↓
Almost no packet loss
````

However, real-world networks can have:

* High latency
* Packet loss
* Jitter
* Low bandwidth
* Packet reordering
* Burst packet loss
* Intermittent connectivity

These conditions can cause:

* Slow application responses
* TCP throughput degradation
* Request timeouts
* Retransmissions
* Failed requests
* Poor user experience

Testing these situations manually on a real network is difficult and can affect other applications.

This project solves the problem by creating an **isolated virtual network** where these conditions can be introduced and measured safely.

---

# 🏗️ Project Architecture

The project has two main parts:

1. **Virtual Network Lab** – creates the client/server networking environment and applies network impairments.
2. **Experiment & Analysis Layer** – generates traffic, measures performance, stores results and creates graphs.

### High-Level Architecture

```text
                         ┌──────────────────────┐
                         │        USER          │
                         │                      │
                         │      chaosctl        │
                         └──────────┬───────────┘
                                    │
                                    ▼
                     ┌──────────────────────────┐
                     │      C++17 Core           │
                     │                          │
                     │  ┌────────────────────┐  │
                     │  │    Emulator        │  │
                     │  │    Profiles        │  │
                     │  │    Scheduler       │  │
                     │  │    Measurements    │  │
                     │  │    Experiments     │  │
                     │  └────────────────────┘  │
                     └─────────────┬────────────┘
                                   │
                                   ▼
                  ┌────────────────────────────────┐
                  │       Linux Network Lab        │
                  │                                │
                  │  ┌────────────┐  ┌──────────┐ │
                  │  │   Client   │  │  Server  │ │
                  │  │ Namespace  │  │Namespace │ │
                  │  │ 10.0.0.1   │  │10.0.0.2  │ │
                  │  └─────┬──────┘  └────┬─────┘ │
                  │        │               │       │
                  │      veth-c           veth-s   │
                  │        │               │       │
                  │        └───────┬───────┘       │
                  │                │               │
                  │          tc / netem            │
                  │                │               │
                  │     Network Impairments        │
                  │                                │
                  │  • Delay                       │
                  │  • Jitter                      │
                  │  • Packet Loss                 │
                  │  • Burst Loss                  │
                  │  • Reordering                  │
                  │  • Duplication                 │
                  │  • Corruption                  │
                  │  • Bandwidth Limiting          │
                  │  • Outage                      │
                  └────────────────┬───────────────┘
                                   │
                                   ▼
                     ┌──────────────────────────┐
                     │      Measurements        │
                     │                          │
                     │  Ping / RTT              │
                     │  iperf3 Throughput       │
                     │  HTTP / curl             │
                     └────────────┬─────────────┘
                                  │
                                  ▼
                         ┌─────────────────┐
                         │   CSV Results   │
                         │                 │
                         │    results/     │
                         └────────┬────────┘
                                  │
                                  ▼
                       ┌────────────────────┐
                       │  plot_results.py  │
                       │                    │
                       │     Pandas         │
                       │     Matplotlib     │
                       └─────────┬──────────┘
                                 │
                                 ▼
                          ┌──────────────┐
                          │    Graphs    │
                          │   plots/     │
                          └──────────────┘
```

---

# 🔍 Architecture Components

## 1. `chaosctl`

`chaosctl` is the command-line interface of the project.

It provides commands to:

* Set up the virtual network
* Apply network profiles
* Apply custom impairments
* Run experiments
* Monitor the network
* Run dynamic chaos scenarios
* Clean up the environment

Example:

```bash
sudo ./build/chaosctl setup
```

---

## 2. Client and Server Namespaces

The project creates two isolated Linux network namespaces.

```text
Client Namespace             Server Namespace
    10.0.0.1                     10.0.0.2
        │                            │
      veth-c                       veth-s
        │                            │
        └──────── Virtual Link ──────┘
```

The namespaces provide an isolated networking environment so that experiments do not modify the host's normal network connection.

---

## 3. Virtual Ethernet Pair

A veth pair acts like a virtual Ethernet cable connecting the two namespaces.

```text
Client Namespace
       │
     veth-c
       │
       │  Virtual Ethernet Link
       │
     veth-s
       │
Server Namespace
```

---

## 4. `tc` and `netem`

Linux Traffic Control (`tc`) and `netem` are used to introduce network impairments.

For example:

```text
Client
   │
   ▼
veth
   │
   ▼
tc / netem
   │
   ├── Add delay
   ├── Add jitter
   ├── Drop packets
   ├── Reorder packets
   ├── Duplicate packets
   └── Limit bandwidth
   │
   ▼
veth
   │
   ▼
Server
```

---

# 📂 Project Structure

```text
Chaos-Emulator-C-Project/
│
├── include/
│   ├── emulator.hpp
│   ├── profiles.hpp
│   ├── scheduler.hpp
│   ├── measure.hpp
│   ├── experiments.hpp
│   └── util.hpp
│
├── src/
│   ├── main.cpp
│   ├── emulator.cpp
│   ├── profiles.cpp
│   ├── scheduler.cpp
│   ├── measure.cpp
│   ├── experiments.cpp
│   └── util.cpp
│
├── tests/
│   └── test_emulator.cpp
│
├── scripts/
│   ├── setup_lab.sh
│   └── teardown_lab.sh
│
├── scenarios/
│
├── results/
│
├── plots/
│
├── plot_results.py
├── scenario.json
├── CMakeLists.txt
├── Makefile
└── README.md
```

---

# 🧩 Main C++ Modules

| Module              | Responsibility                                   |
| ------------------- | ------------------------------------------------ |
| `main.cpp`          | Implements the `chaosctl` command-line interface |
| `emulator.cpp`      | Applies and removes network impairments          |
| `profiles.cpp`      | Contains predefined network profiles             |
| `scheduler.cpp`     | Handles random, flap and timeline-based chaos    |
| `measure.cpp`       | Runs ping, iperf3 and HTTP measurements          |
| `experiments.cpp`   | Runs the benchmark experiments                   |
| `util.cpp`          | Provides utility and process-execution functions |
| `test_emulator.cpp` | Unit tests for core functionality                |

---

# 🌐 Network Impairments

The emulator can simulate different network conditions.

### Latency

Adds a delay to packet delivery.

```text
Normal:
Client ─────────→ Server

With latency:
Client ── delay ──→ Server
```

### Jitter

Introduces variation in packet delay.

```text
Packet 1 → 40 ms
Packet 2 → 70 ms
Packet 3 → 50 ms
Packet 4 → 90 ms
```

### Packet Loss

Randomly drops packets.

```text
✓ ✓ ✗ ✓ ✗ ✓ ✓ ✗
```

### Burst Loss

Creates consecutive packet losses.

```text
✓ ✓ ✓ ✗ ✗ ✗ ✓ ✓
```

### Packet Reordering

Changes the order of packets.

```text
Sent:     1 → 2 → 3 → 4

Received: 1 → 3 → 2 → 4
```

### Packet Duplication

Duplicates packets.

```text
Sent:     1 → 2 → 3

Received: 1 → 2 → 2 → 3
```

### Bandwidth Limiting

Restricts the available network bandwidth.

### Outage

Simulates complete network unavailability.

---

# 📋 Predefined Network Profiles

The project provides predefined profiles for commonly encountered network conditions.

```text
clean
broadband
4g
3g
bad-wifi
satellite
disaster
outage
```

Example:

```bash
sudo ./build/chaosctl profile 3g
```

These profiles are simulated configurations and are intended to approximate different types of network environments.

---

# ⚙️ Custom Network Conditions

Network conditions can also be specified manually.

Example:

```bash
sudo ./build/chaosctl apply --delay 100 --jitter 20 --loss 5
```

This configures approximately:

```text
Delay  : 100 ms
Jitter : 20 ms
Loss   : 5%
```

---

# 🔄 Dynamic Chaos

The project supports changing network conditions while an experiment is running.

### Random Mode

```bash
sudo ./build/chaosctl random
```

Random impairments can be applied over time.

### Flapping Mode

```bash
sudo ./build/chaosctl flap
```

The network can repeatedly transition between available and unavailable states.

### Timeline Mode

A scenario file can define a sequence of network conditions.

```text
Normal
  ↓
High Latency
  ↓
Packet Loss
  ↓
Recovery
```

---

# 📊 Measurements

The project measures the effect of network conditions using:

### Ping

Used to measure:

* RTT
* Packet loss

### iperf3

Used to measure:

* TCP throughput
* UDP throughput

### HTTP / curl

Used to measure:

* HTTP request/response behaviour
* Application-level performance

---

# 🧪 Experiments

The project contains several experiment categories:

```text
1. Latency
2. Loss
3. TCP vs Loss
4. TCP vs RTT
5. Network Profiles
6. Burst vs Random Loss
7. Dynamic Chaos
8. Application Resilience
```

---

# 📈 Results and Graphs

Experiment results are stored as CSV files:

```text
results/
```

The Python plotting script processes these results:

```bash
python3 plot_results.py
```

The script uses:

* Pandas
* Matplotlib

and generates graphs inside:

```text
plots/
```

The graphs help compare:

* Configured vs measured latency
* Configured vs measured packet loss
* TCP throughput vs packet loss
* TCP throughput vs RTT
* Performance across network profiles
* Random vs burst loss
* Dynamic network behaviour
* Resilience of different clients

---

# 📐 TCP and Mathis Model

The project also compares measured TCP throughput with the approximate Mathis model:

```text
BW ≤ (MSS / RTT) × (1.22 / √p)
```

Where:

```text
MSS = Maximum Segment Size
RTT = Round Trip Time
p   = Packet Loss Probability
```

This helps compare theoretical TCP behaviour with experimentally measured throughput.

---

# 🔬 Example Workflow

A typical experiment follows this workflow:

```text
1. Build the project
        ↓
2. Setup the virtual network
        ↓
3. Start client/server environment
        ↓
4. Apply a network profile
        ↓
5. Generate network traffic
        ↓
6. Measure performance
        ↓
7. Save measurements to CSV
        ↓
8. Generate graphs
        ↓
9. Analyze the results
        ↓
10. Teardown the virtual network
```

---

# 🚀 Build

Requirements:

* Linux
* C++17
* CMake
* iproute2
* iperf3
* Python 3
* Pandas
* Matplotlib
* Root privileges for network namespace operations

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

---

# ▶️ Basic Usage

### Setup

```bash
sudo ./build/chaosctl setup
```

### Apply a profile

```bash
sudo ./build/chaosctl profile 3g
```

### Apply custom impairments

```bash
sudo ./build/chaosctl apply --delay 100 --jitter 20 --loss 5
```

### Monitor RTT

```bash
sudo ./build/chaosctl monitor --duration 30
```

### Run experiments

```bash
sudo ./build/chaosctl experiment latency
```

Other experiment names can be used depending on the available experiment commands.

### Generate plots

```bash
python3 plot_results.py
```

### Cleanup

```bash
sudo ./build/chaosctl teardown
```

---

# 🛡️ Isolation and Safety

The project is designed to perform experiments inside Linux network namespaces instead of modifying the host's normal network interface.

This allows network conditions to be tested without intentionally disrupting the computer's regular network connection.

The project also includes cleanup handling for terminating the emulator and restoring the virtual lab.

---

# 🎯 Key Takeaways

The project demonstrates how to:

* Create an isolated Linux networking environment
* Connect namespaces using virtual Ethernet interfaces
* Use `tc/netem` to emulate unreliable networks
* Generate controlled network traffic
* Measure latency, packet loss and throughput
* Study TCP behaviour under network impairments
* Compare experimental results with theoretical models
* Test application resilience
* Visualize experimental results using Python

---

# 🔮 Future Improvements

Possible future improvements include:

* Web-based monitoring dashboard
* More application protocols
* More realistic traffic patterns
* Distributed testing across multiple machines
* Additional network profiles
* Real-time graph visualization
* More advanced resilience testing

---

# 📄 License

This project is intended for educational and experimental purposes.

```

### One thing I would especially keep

The **architecture diagram near the top** is valuable because when your mentor opens the GitHub repository, they can immediately understand:

**`chaosctl → C++ modules → Linux namespaces → veth → tc/netem → traffic → measurements → CSV → graphs`**

That is essentially the complete story of your project in one picture.
```
