# V12.1 — Saturating offered-load experiment

The V12 packet-level prototype currently sends one 1200-byte packet every 1 ms, which creates an application offer rate of about 9.6 Mbps. That ceiling masks any benefit from combining the 30/20/15 Mbps backhauls.

## Goal

Increase offered traffic above a single-link capacity and compare:

- bonding enabled: weighted striping over all three links;
- bonding disabled: traffic restricted to Link 1.

## Required measurements

- received packets
- delivered packets
- dropped packets
- reordered packets
- effective application goodput

## Interpretation

V12 already demonstrates packet striping and substantial out-of-order arrival caused by heterogeneous link delay. V12.1 is intended to test whether the scheduler can turn the additional path capacity into measured goodput.

A numerical aggregation claim is valid only after the local ns-3 run confirms a higher bonded goodput under a saturating offered load.