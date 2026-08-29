# V12 — Packet-Level Multi-Link Bonding Prototype

V12 is the first ns-3 implementation in this project that models packet-level striping across three backhaul paths and receiver-side in-order reassembly.

## Model

- Link 1: 30 Mbps, 20 ms
- Link 2: 20 Mbps, 35 ms
- Link 3: 15 Mbps, 50 ms
- Weighted packet scheduler: 6:4:3
- Per-packet sequence number
- Receiver reordering buffer
- Duplicate/late packet detection
- Delivered-byte goodput calculation

## Run

Copy `v12/railway_v12_bonding.cc` into the ns-3 `scratch/railway/` directory and build it there.

The program supports:

- `--bonding=true|false`
- `--packetBytes=<bytes>`
- `--simulationSeconds=<seconds>`
- `--seed=<seed>`

## Validation status

The source has been added to the repository but must be compiled and executed in the local ns-3.48 environment before its numerical results are considered validated.

This is a research prototype, not a production multipath transport implementation such as MPTCP, MPQUIC or a commercial SD-WAN bonding system.
