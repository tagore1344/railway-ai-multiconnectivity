# V17 Hardware Gateway Specification

## Control plane
- Linux edge computer
- 3+ cellular modems/interfaces
- local ML inference service
- watchdog and modem reset controller

## Data plane
- packet sequence/flow ID
- scheduler and path health table
- bonding tunnel (future production implementation should use a standards-based multipath transport or controlled tunnel)
- receiver reorder/recovery buffer

## Operational safeguards
- emergency traffic priority
- fail-closed routing when all links are unusable
- bounded queues and memory
- telemetry without storing passenger payloads by default

## Validation gate
Hardware claims are not considered validated until measurements are collected from real modems and a reproducible railway/route test.
