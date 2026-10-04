#!/usr/bin/env bash
# Kills lab servers and deletes namespaces (this also removes the veth pair + all netem rules).
[ "$EUID" -eq 0 ] || { echo "Run with sudo"; exit 1; }
for n in client server; do
  if ip netns list | grep -qw "$n"; then
    ip netns pids "$n" | xargs -r kill 2>/dev/null || true
    ip netns del "$n" || true
  fi
done
echo "Lab removed."
