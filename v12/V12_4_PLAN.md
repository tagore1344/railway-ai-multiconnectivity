# V12.4 — Latency-aware striping and bounded reordering

## Goal
Reduce head-of-line blocking and reorder-buffer growth observed in V12.3 while preserving high aggregate received goodput.

## Current V12.3 observation
The local V12.3 run reached about 62.7 Mbps unique received goodput with bonding enabled, but reported a 98.3% reorder rate and a peak reorder buffer of 164,819 packets. These numbers indicate that the fixed 6:4:3 scheduler is not sufficiently matched to heterogeneous path delays.

## V12.4 design
1. Keep the sequence-numbered packet format.
2. Replace fixed 6:4:3 striping with latency-aware weighted scheduling.
3. Prefer paths with a smaller predicted arrival-time error relative to the current aggregate path arrival time.
4. Bound the reorder buffer.
5. Use a bounded gap timeout tied to the configured path-delay spread.
6. Record buffer occupancy, timeout events, and delivered bytes separately from unique received bytes.

## Research metrics
- aggregate unique received goodput
- in-order application goodput
- reorder rate
- maximum reorder-buffer occupancy
- gap timeout count
- declared missing sequence count
- packets per link

## Claim boundary
V12.4 remains an ns-3 research prototype. It is not a production Internet bonding protocol and should not be described as MPTCP, MPQUIC, or commercial-grade link aggregation.
