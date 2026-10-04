#!/usr/bin/env bash
# Creates an isolated lab: [client ns] --veth-- [server ns]. Safe: never touches your real NIC.
set -euo pipefail
[ "$EUID" -eq 0 ] || { echo "Run with sudo"; exit 1; }
DIR="$(cd "$(dirname "$0")" && pwd)"
"$DIR/teardown_lab.sh" >/dev/null 2>&1 || true
modprobe sch_netem 2>/dev/null || echo "WARNING: could not load sch_netem (kernel may lack it)"

ip netns add client
ip netns add server
ip link add veth-c type veth peer name veth-s
ip link set veth-c netns client
ip link set veth-s netns server
ip netns exec client ip addr add 10.0.0.1/24 dev veth-c
ip netns exec server ip addr add 10.0.0.2/24 dev veth-s
ip netns exec client ip link set veth-c up
ip netns exec server ip link set veth-s up
ip netns exec client ip link set lo up
ip netns exec server ip link set lo up

# Turn off offloads so netem sees real 1500-byte packets (otherwise loss/rate are inaccurate)
ip netns exec client ethtool -K veth-c tso off gso off gro off 2>/dev/null || true
ip netns exec server ethtool -K veth-s tso off gso off gro off 2>/dev/null || true

# Test payloads for the HTTP experiments
mkdir -p /tmp/testfiles
[ -f /tmp/testfiles/file ]  || head -c 2097152 /dev/urandom > /tmp/testfiles/file    # 2 MB
[ -f /tmp/testfiles/small ] || head -c 51200   /dev/urandom > /tmp/testfiles/small   # 50 KB

echo "Lab ready. Baseline ping:"
ip netns exec client ping -c 3 -q 10.0.0.2 | tail -2
