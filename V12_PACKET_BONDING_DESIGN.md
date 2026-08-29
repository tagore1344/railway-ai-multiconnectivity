# V12 — Packet-Level Bonding Design

## Objective

Move from the V10 offline capacity model to a packet-level bonding prototype.

## Data path

```text
Passenger traffic
      |
      v
Gateway ingress
      |
      +--> Scheduler --> Link 1 ---->\
      |                              \
      +--> Scheduler --> Link 2 ------> Receiver
      |                              /
      +--> Scheduler --> Link 3 ---->/
                                     |
                              sequence/reorder
                                     |
                                  goodput
```

## Mechanisms

- Monotonic packet sequence numbers.
- Weighted packet striping across paths.
- Independent path delay and capacity.
- Receiver-side out-of-order retention and in-order delivery.
- Duplicate sequence detection.
- Effective goodput measurement.

## Limitations

This is a research prototype, not production MPTCP, MPQUIC, or SD-WAN bonding. It does not yet implement coupled congestion control, production retransmission logic, encryption/session migration, NAT traversal, or hardware dataplane behavior.

## Validation plan

Compare best-link, multi-link striping, and predictive multi-link scheduling under identical scenarios. Report goodput, loss, duplicates, out-of-order packets, path switches, and path-failure recovery time.

## Local validation

`ns3/railway_v12_bonding.cc` is the self-contained ns-3 prototype. It still needs compilation and runtime validation in the local ns-3 tree before its results can be treated as validated experimental evidence.
