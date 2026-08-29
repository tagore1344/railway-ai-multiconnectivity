# AI-Driven Multi-Connectivity Edge Gateway for High-Mobility Railway Wi-Fi

## Abstract

Reliable passenger Internet connectivity on moving trains is challenging because the quality of available backhaul links changes with position, channel conditions, load, and failures. This project develops a simulation-based railway edge gateway that monitors multiple heterogeneous links, performs adaptive load balancing and failover, and explores predictive network selection with machine learning. Using ns-3, the project progresses from a three-link baseline to mobility-aware adaptive control and randomized future-link prediction. A Random Forest model trained on earlier randomized simulation seeds achieved 99.24% accuracy when predicting the next selected network on 132 observations from 12 unseen seeds. The result demonstrates feasibility within the synthetic simulation model, while the project explicitly separates simulation evidence from real-world performance claims. A later stage introduces offline multi-link bonding capacity modeling and integrates prediction with bonding. Future work is to implement packet-level striping, sequencing, reordering/reassembly, retransmission handling, and measured goodput, followed by hardware and real-network validation.

**Keywords:** railway Wi-Fi, multi-connectivity, edge gateway, network selection, mobility-aware networking, machine learning, failover, multi-link bonding

## 1. Introduction

Passenger Wi-Fi on high-mobility railway systems must remain usable while the train moves through changing coverage areas. A single backhaul connection can degrade or fail, while other networks may remain usable. The project therefore investigates an onboard edge gateway that continuously observes several candidate links and chooses how passenger traffic should be distributed.

The research is organized as an incremental simulation and machine-learning prototype. Early versions establish multi-connectivity, load balancing, failover, and train mobility. Later versions introduce a composite link score and randomized network conditions. The machine-learning stage asks whether current measurements can predict the network that will be selected at the next controller interval. The final stages investigate whether predictive selection can be combined with multi-link capacity aggregation.

## 2. Research Contributions

1. A modular railway multi-connectivity gateway model for ns-3.
2. Capacity-aware passenger load balancing across three heterogeneous links.
3. Automatic failover when a backhaul link becomes unavailable.
4. Mobility-aware link-quality modeling as the train moves along a route.
5. A randomized simulation dataset suitable for future-link prediction experiments.
6. An unseen-seed Random Forest evaluation for next-network prediction.
7. Offline predictive-plus-bonding capacity analysis.

## 3. System Architecture

The gateway contains four conceptual functions: link monitoring, traffic management, prediction, and multi-link optimization. Link monitoring collects quality, latency, packet-loss, and load indicators. The controller converts those indicators into a selection decision. The prediction layer estimates the next preferred link before the next observation arrives. The bonding layer estimates the benefit of using multiple simultaneously usable links.

## 4. Simulation Methodology

The simulation uses ns-3 with three heterogeneous backhaul networks. Nominal capacities are 30 Mbps, 20 Mbps, and 15 Mbps, with nominal delays of 20 ms, 35 ms, and 50 ms. The train moves at 20 m/s in the reference scenario. The route model uses three base-station positions at 200 m, 500 m, and 800 m.

The V7 dataset was generated from 40 combinations of train speed, passenger count, and controller interval and contains 840 observations. V8.2 expanded the experiment to 60 independent random seeds and produced 720 observations.

## 5. Adaptive Gateway Controller

The V6 controller combines link quality, capacity, latency, packet loss, and passenger load into an adaptive score. As the train moves, the best-scoring network changes. The controller therefore redistributes passenger load toward currently favorable networks.

## 6. Machine-Learning Prediction

The V8.2 experiment predicts the network selected at the next controller interval from information available at the current interval. The model excludes the current selected-link label and controller-generated score fields. Training uses seeds below 1049 and evaluation uses seeds 1049 and above.

The Random Forest experiment used 528 training observations and 132 test observations from 12 unseen seeds. The measured test accuracy was 99.24%, with class-wise recall of 1.00, 0.97, and 1.00 for networks 1, 2, and 3 respectively.

## 7. Interpretation of Prediction Results

The strongest feature in the V8.2 model was train position, followed by network load and link-quality/loss measurements. The high accuracy is evidence of predictability in the current synthetic simulation model. It is not evidence that a deployed railway system would achieve the same accuracy. Real cellular networks exhibit additional temporal, spatial, scheduling, interference, handover, and infrastructure effects that are not captured by the current synthetic model.

## 8. Predictive vs Reactive Control

The V9 stage evaluates whether predicting the next preferred network can prepare the gateway before a reactive controller observes the change. The initial V9 evaluator is an offline replay because the V8.2 dataset does not contain per-interval throughput. Its utility comparison therefore uses the synthetic controller score rather than claiming throughput improvement.

A final V9 ns-3 experiment should measure throughput, latency, packet loss, switching count, and time spent on degraded links under identical randomized scenarios.

## 9. Multi-Link Bonding

The V10 stage estimates the capacity benefit of pooling usable link capacities. V11 combines this bonding-capacity model with the predictive network-selection model. These stages are intentionally labeled capacity/utilization models because they do not yet provide packet-level Internet bonding.

## 10. Limitations

The current evidence is simulation-based. Link quality and packet loss are synthetic functions of position and controlled randomness. The traffic model is simplified. No real cellular modem measurements are used, and the project does not yet implement a production bonding tunnel.

## 11. Future Work

The next engineering stage is packet-level multi-link bonding. The planned pipeline is packet scheduling, sequence numbering, link-aware striping, receiver reordering/reassembly, retransmission control, and effective goodput measurement. The predictive controller can then be integrated with the bonding scheduler to reduce the amount of traffic placed on an anticipated-to-degrade link.

After simulation validation, a hardware proof of concept can be developed using a Linux edge computer and multiple independent backhaul interfaces. Real route measurements can then be collected to validate the model before any commercial performance claims are made.

## 12. Conclusion

The project demonstrates an incremental approach to resilient railway passenger connectivity. The simulation progressed from simple multi-connectivity and failover to mobility-aware control and machine-learning-based future-link prediction. The strongest current result is a 99.24% next-network prediction accuracy on unseen randomized simulation seeds. The next decisive research milestone is not another classification score, but demonstrating that predictive decisions combined with packet-level bonding improve measured goodput and resilience under realistic mobility and failure scenarios.

## References to Add Before Submission

The final paper should include verified academic references covering: railway passenger connectivity, heterogeneous wireless network selection, multi-WAN or multipath transport, mobility-aware prediction, machine-learning-based link prediction, ns-3 methodology, and packet-level bonding protocols such as MPTCP or multipath QUIC where applicable.
