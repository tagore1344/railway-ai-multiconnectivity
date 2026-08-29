# V11 — Predictive + Multi-Link Bonding

V11 combines the two main research directions developed so far:

1. V8.2 future network prediction.
2. V10 multi-link bonding capacity modeling.

## What V11 does

The model trains a Random Forest on seeds `< 1049` and predicts the next selected network on unseen seeds `>= 1049`. It then compares the capacity available through:

- the current reactive selection;
- the predicted next network;
- all simultaneously usable links as a bonded-capacity upper bound.

## Outputs

`v11-predictive-bonding-results.json` contains predictive accuracy and modeled capacity comparisons.

## Important limitation

V11 is an **offline synthetic simulation model**. The bonding component is not a real packet-level bonding tunnel. It does not yet implement packet striping, packet sequence numbering on a shared tunnel, receiver reordering/reassembly, retransmission control, MPTCP/MPQUIC, or a production SD-WAN implementation.

Therefore V11 must not be presented as proof of real-world Internet speed aggregation. The next engineering milestone is V12: packet-level ns-3 integration with measured goodput, latency, loss, reordering, and failover behavior.
