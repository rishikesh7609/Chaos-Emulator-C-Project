"""Turns results/*.csv into plots/*.png (mean +/- std over repetitions)."""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

RES, OUT = "results", "plots"


def _load(name):
    p = f"{RES}/{name}.csv"
    return pd.read_csv(p) if os.path.exists(p) and os.path.getsize(p) > 0 else None


def _save(fig, name):
    os.makedirs(OUT, exist_ok=True)
    fig.tight_layout(); fig.savefig(f"{OUT}/{name}.png", dpi=150); plt.close(fig)
    print("saved", f"{OUT}/{name}.png")


def _line(df, x, ycols, title, xl, yl, name, logx=False, logy=False, ideal=None):
    g = df.groupby(x).agg(["mean", "std"])
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for col, label, style in ycols:
        if col in g.columns.get_level_values(0):
            ax.errorbar(g.index, g[col]["mean"], yerr=g[col]["std"].fillna(0), marker="o",
                        capsize=3, label=label, linestyle=style)
    if ideal is not None:
        ax.plot(g.index, ideal(g.index), "k--", alpha=.5, label="ideal")
    if logx: ax.set_xscale("symlog", linthresh=0.1)
    if logy: ax.set_yscale("log")
    ax.set(title=title, xlabel=xl, ylabel=yl); ax.grid(alpha=.3); ax.legend()
    _save(fig, name)


def plot_latency():
    d = _load("latency")
    if d is not None:
        _line(d, "expected_rtt_ms", [("rtt_avg", "measured RTT", "-")], "Latency validation: configured vs measured RTT",
              "Expected RTT (ms) = 2 x one-way delay", "Measured RTT (ms)", "01_latency_validation", ideal=lambda x: x)


def plot_loss():
    d = _load("loss")
    if d is not None:
        _line(d, "expected_roundtrip_loss", [("loss", "measured", "-")], "Loss validation: configured vs measured",
              "Expected round-trip loss (%)", "Measured loss (%)", "02_loss_validation", ideal=lambda x: x)


def plot_tcp_loss():
    d = _load("tcp_loss")
    if d is not None:
        _line(d, "loss_pct", [("tcp_mbps", "measured (iperf3)", "-"), ("mathis_mbps", "Mathis model", "--")],
              "TCP throughput vs packet loss (RTT ~50 ms)", "Packet loss (%)", "Throughput (Mbit/s)",
              "03_tcp_vs_loss", logx=True, logy=True)


def plot_tcp_rtt():
    d = _load("tcp_rtt")
    if d is not None:
        _line(d, "rtt_ms", [("tcp_mbps", "measured (iperf3)", "-"), ("mathis_mbps", "Mathis model", "--")],
              "TCP throughput vs RTT (0.5% loss)", "RTT (ms)", "Throughput (Mbit/s)", "04_tcp_vs_rtt", logy=True)


def plot_profiles():
    d = _load("profiles")
    if d is None: return
    order = list(dict.fromkeys(d["profile"]))
    g = d.groupby("profile").agg(["mean", "std"]).loc[order]
    fig, axes = plt.subplots(1, 3, figsize=(14, 4.2))
    for ax, (col, lab) in zip(axes, [("http_s", "2 MB page load (s)"), ("tcp_mbps", "TCP throughput (Mbit/s)"),
                                     ("rtt_avg", "Avg RTT (ms)")]):
        ax.bar(order, g[col]["mean"], yerr=g[col]["std"].fillna(0), capsize=3)
        ax.set_title(lab); ax.tick_params(axis="x", rotation=45); ax.set_yscale("log"); ax.grid(alpha=.3, axis="y")
    _save(fig, "05_profiles")


def plot_burst():
    d = _load("burst")
    if d is None: return
    g = d.groupby(["avg_loss_pct", "model"])["tcp_mbps"].agg(["mean", "std"]).unstack()
    fig, ax = plt.subplots(figsize=(7, 4.5))
    g["mean"].plot.bar(yerr=g["std"].fillna(0), ax=ax, capsize=3)
    ax.set(title="Random vs bursty loss (same average)", xlabel="Average loss (%)", ylabel="TCP throughput (Mbit/s)")
    ax.grid(alpha=.3, axis="y"); plt.setp(ax.get_xticklabels(), rotation=0)
    _save(fig, "07_burst_vs_random")


def plot_chaos():
    ping, ip, ev = _load("chaos_ping"), _load("chaos_iperf"), _load("chaos_events")
    if ping is None or ev is None: return
    t0 = ev["timestamp"].iloc[0]
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(11, 6), sharex=True)
    ok = ping.dropna()
    a1.plot(ok["timestamp"] - t0, ok["rtt_ms"], ".-", ms=3, lw=.7, label="RTT")
    lost = ping[ping["rtt_ms"].isna()]
    a1.plot(lost["timestamp"] - t0, [0] * len(lost), "rx", label="lost probe")
    a1.set_ylabel("RTT (ms)"); a1.legend(loc="upper left")
    if ip is not None: a2.plot(ip["timestamp"] - t0, ip["tcp_mbps"], "g.-"); a2.set_ylabel("TCP Mbit/s")
    a2.set_xlabel("Time (s)")
    cmap = plt.get_cmap("Set3")
    for i in range(len(ev) - 1):
        s, e = ev["timestamp"].iloc[i] - t0, ev["timestamp"].iloc[i + 1] - t0
        for ax in (a1, a2): ax.axvspan(s, e, color=cmap(i % 12), alpha=.35)
        a1.text((s + e) / 2, a1.get_ylim()[1] * .95, ev["label"].iloc[i], ha="center", va="top", fontsize=8)
    a1.set_title("Chaos timeline: RTT and throughput vs active impairment")
    for ax in (a1, a2): ax.grid(alpha=.3)
    _save(fig, "06_chaos_timeline")


def plot_resilience():
    d = _load("resilience")
    if d is None: return
    g = d.groupby(["profile", "client"])["ok"].mean().mul(100).unstack()
    order = [p for p in ["clean", "4g", "bad_wifi", "disaster"] if p in g.index]
    fig, ax = plt.subplots(figsize=(7, 4.5)); g.loc[order].plot.bar(ax=ax)
    ax.set(title="Success within 6 s: naive vs resilient client", ylabel="Success rate (%)", xlabel="")
    plt.setp(ax.get_xticklabels(), rotation=0); ax.grid(alpha=.3, axis="y")
    _save(fig, "08_resilience")


def plot_all():
    for f in (plot_latency, plot_loss, plot_tcp_loss, plot_tcp_rtt, plot_profiles, plot_chaos, plot_burst, plot_resilience):
        try: f()
        except Exception as e: print(f"[warn] {f.__name__} failed: {e}")


if __name__ == "__main__":
    plot_all()
