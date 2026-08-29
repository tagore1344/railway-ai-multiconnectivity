# V12 — Packet-Level Bonding Design

## Objective

Move from the V10/V11 offline capacity models to a real packet-level multi-link mechanism that can be implemented in ns-3 and later mapped to a Linux multi-WAN gateway.

## Data path

```text
Passenger traffic
      |
      v
+-----------------------+
| Bonding scheduler     |
| - link eligibility    |
| - weighted scheduling |
| - sequence numbering  |
+-----------+-----------+
            |
     +------+------+
     |      |      |
     v      v      v
   Link 1  Link 2  Link 3
     |      |      |
     +------+------+
            |
            v
+-----------------------+
| Reordering/reassembly |
| - sequence window     |
| - duplicate filtering |
| - loss/retransmit     |
+-----------+-----------+
            |
            v
      Application sink
```

## Scheduler requirements

1. Assign a monotonically increasing sequence number to each data segment.
2. Estimate per-link delay, capacity, loss, and queue state.
3. Select a link for each segment using a weighted policy.
4. Prevent a slow link from creating an unbounded receiver reorder queue.
5. Record transmission time and link identity for every segment.

## Receiver requirements

1. Accept segments in arbitrary arrival order.
2. Buffer out-of-order segments.
3. Deliver only the next contiguous sequence range to the application.
4. Detect gaps and request or schedule retransmission when supported.
5. Track reorder-buffer occupancy and head-of-line delay.

## Metrics

- aggregate goodput
- application delivery rate
- packet loss
- retransmissions
- out-of-order arrivals
- reorder-buffer occupancy
- head-of-line delay
- per-link utilization
- failover time
- number of link switches

## Research comparison

The main experiment should compare:

- single best link;
- reactive multi-link scheduler;
- predictive scheduler;
- predictive scheduler + bonding;
- oracle scheduler.

All policies must use identical randomized channel traces and traffic demand.

## Validation rule

A V12 implementation should not be called "true bonding" until application-level goodput is measured from the receiver after packets have been scheduled over multiple links and successfully reordered/reassembled.
