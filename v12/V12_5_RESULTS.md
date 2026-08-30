# V12.5 Experimental Results

## Local ns-3.48 run

The V12.5 source compiled and executed successfully in the user's local ns-3.48 environment.

### Bonding enabled
- Offered application rate: 64 Mbps (the program's displayed `64000 Mbps` is a formatting/unit bug and should not be used as a scientific result)
- Unique received goodput: 61.7514 Mbps
- In-order goodput: 59.8382 Mbps
- Reordered packets: 191,789
- Reorder rate: 99.7187%
- Gap timeout events: 5,422
- Declared lost packets: 5,470
- Maximum reorder buffer: 6,001 packets

### Bonding disabled (single-link baseline)
- Offered application rate: 64 Mbps
- Unique received goodput: 28.966 Mbps
- In-order goodput: 27.0396 Mbps
- Reordered packets: 89,704
- Reorder rate: 99.4314%
- Gap timeout events: 51,995
- Declared lost packets: 85,851
- Maximum reorder buffer: 6,001 packets

## Interpretation

V12.5 demonstrates that the bounded receiver prevents unbounded reorder-buffer growth and that the bonded configuration retains substantially higher aggregate goodput than the single-link baseline. Bonding enabled achieves approximately 2.13x the unique received goodput of the baseline (61.7514 / 28.966). In-order goodput is also substantially higher (59.8382 vs 27.0396 Mbps).

The very high reorder rates show that the current simplified packet-level model still produces strong path-order distortion. The declared-loss counters are model-level timeout accounting, not measured physical packet loss; this distinction must be preserved in the paper.

The reported V12.5 values are local experimental observations supplied by the project operator, not independently reproduced here.
