#!/usr/bin/env sh
# Fallback build without CMake:  ./build.sh   ->  build/chaosctl , build/test_emulator
set -e
mkdir -p build
SRC="src/util.cpp src/emulator.cpp src/profiles.cpp src/scheduler.cpp src/measure.cpp src/experiments.cpp"
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude $SRC src/main.cpp -pthread -o build/chaosctl
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude $SRC tests/test_emulator.cpp -pthread -o build/test_emulator
echo "built: build/chaosctl build/test_emulator"
