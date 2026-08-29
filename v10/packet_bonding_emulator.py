from __future__ import annotations

"""V10 packet-level bonding prototype.

This is a discrete-event/offline emulator, not a kernel networking stack.
It models sequence numbers, weighted striping across links, independent
latency/loss, receiver reordering, retransmission of lost packets, and
measured application goodput.
"""

import argparse
import heapq
import random
from dataclasses import dataclass

import pandas as pd

CAPACITY = {1: 30.0, 2: 20.0, 3: 15.0}
DELAY_MS = {1: 20.0, 2: 35.0, 3: 50.0}
QUALITY = {1: "n1_quality", 2: "n2_quality", 3: "n3_quality"}
LOSS = {1: "n1_loss", 2: "n2_loss", 3: "n3_loss"}

@dataclass(order=True)
class PacketArrival:
    arrival_ms: float
    sequence: int
    size_bytes: int
    link: int
    retransmission: bool = False


def run_trace(row: pd.Series, packets: int, packet_bytes: int, seed: int) -> dict:
    rng = random.Random(seed)
    links = []
    for link in (1, 2, 3):
        q = max(0.0, min(1.0, float(row[QUALITY[link]])))
        loss = max(0.0, min(100.0, float(row[LOSS[link]]))) / 100.0
        if q > 0.05:
            links.append((link, CAPACITY[link] * q * (1.0 - loss), DELAY_MS[link], loss))

    if not links:
        return {"delivered": 0, "goodput_mbps": 0.0, "reordered": 0, "retransmitted": 0}

    heap: list[PacketArrival] = []
    sent = 0
    retransmitted = 0
    round_robin = 0

    # Weighted striping: choose proportionally to effective link capacity.
    weights = [x[1] for x in links]
    total_weight = sum(weights)

    for seq in range(packets):
        r = rng.random() * total_weight
        cumulative = 0.0
        chosen = links[-1]
        for item in links:
            cumulative += item[1]
            if r <= cumulative:
                chosen = item
                break
        link, _, delay_ms, loss = chosen
        sent += 1
        tx_ms = packet_bytes * 8.0 / (CAPACITY[link] * 1_000_000.0) * 1000.0
        if rng.random() < loss:
            retransmitted += 1
            # Fast synthetic retransmission over the best currently usable path.
            best = max(links, key=lambda x: x[1])
            tx_ms = packet_bytes * 8.0 / (CAPACITY[best[0]] * 1_000_000.0) * 1000.0
            arrival = delay_ms + tx_ms + best[2]
            if rng.random() < best[3]:
                continue
            heapq.heappush(heap, PacketArrival(arrival, seq, packet_bytes, best[0], True))
        else:
            heapq.heappush(heap, PacketArrival(delay_ms + tx_ms, seq, packet_bytes, link))
        round_robin += 1

    next_expected = 0
    delivered = 0
    reordered = 0
    buffer: dict[int, PacketArrival] = {}
    last_arrival = 0.0

    while heap:
        pkt = heapq.heappop(heap)
        last_arrival = max(last_arrival, pkt.arrival_ms)
        if pkt.sequence != next_expected:
            reordered += 1
        buffer[pkt.sequence] = pkt
        while next_expected in buffer:
            delivered += buffer.pop(next_expected).size_bytes
            next_expected += 1

    # Application goodput uses delivered in-order bytes over the elapsed arrival time.
    goodput = delivered * 8.0 / max(last_arrival, 1.0) / 1_000.0
    return {
        "delivered": delivered,
        "goodput_mbps": goodput,
        "reordered": reordered,
        "retransmitted": retransmitted,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", default="railway-v8.2-dataset.csv")
    parser.add_argument("--packets", type=int, default=5000)
    parser.add_argument("--packet-bytes", type=int, default=1200)
    parser.add_argument("--rows", type=int, default=100)
    args = parser.parse_args()

    df = pd.read_csv(args.dataset).head(args.rows)
    results = []
    for i, row in df.iterrows():
        result = run_trace(row, args.packets, args.packet_bytes, 10000 + i)
        result.update({"seed": int(row["seed"]), "time": float(row["time"])})
        results.append(result)

    out = pd.DataFrame(results)
    out.to_csv("v10-packet-bonding-results.csv", index=False)
    print("=================================================")
    print(" Railway V10 — Packet Bonding Prototype")
    print("=================================================")
    print(f"Traces             : {len(out)}")
    print(f"Mean goodput       : {out['goodput_mbps'].mean():.3f} Mbps")
    print(f"Max goodput        : {out['goodput_mbps'].max():.3f} Mbps")
    print(f"Mean reordered     : {out['reordered'].mean():.2f} packets/trace")
    print(f"Mean retransmitted : {out['retransmitted'].mean():.2f} packets/trace")
    print("Saved              : v10-packet-bonding-results.csv")
    print("NOTE: offline packet/reordering prototype; not a real MPTCP/MPQUIC implementation.")

if __name__ == "__main__":
    main()
