# V10 — Multi-Link Bonding Capacity Emulation

## Purpose

V10 estimates the potential benefit of combining several simultaneously usable backhaul links for the same passenger traffic demand.

## Compared policies

1. **Best-link:** all demand uses the strongest available link.
2. **Multi-link:** usable capacity from several links is available to the aggregate passenger demand.
3. **Bonded-capacity model:** the usable capacity of all reachable links is pooled into one aggregate capacity estimate.

## Inputs

V10 uses the randomized V8.2 dataset:

- train position
- per-network quality
- per-network packet-loss estimate
- three nominal network capacities

## Important limitation

This version is an **offline capacity/utilization emulation**. It does **not** yet implement packet-level Internet bonding. It therefore does not claim true packet striping, reordering, retransmission, MPTCP, MPQUIC, or SD-WAN bonding.

## Next engineering milestone

Implement packet-level striping and reassembly in the ns-3 simulation or through a real multi-WAN bonding stack, then compare:

- aggregate throughput
- packet loss
- latency
- reordering overhead
- failover time
- effective goodput

against the V10 capacity model.
