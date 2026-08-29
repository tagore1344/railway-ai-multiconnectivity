#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

struct NetworkInfo
{
    double capacityMbps;
    double basePosition;
    double range;
    std::string name;
};

int
main(int argc, char* argv[])
{
    uint32_t passengerCount = 12;
    double simulationTime = 60.0;
    double decisionInterval = 5.0;
    double passengerDemandMbps = 1.0;
    double trainSpeed = 20.0;

    CommandLine cmd;

    cmd.AddValue(
        "passengers",
        "Number of logical passenger flows",
        passengerCount);

    cmd.AddValue(
        "time",
        "Simulation duration in seconds",
        simulationTime);

    cmd.AddValue(
        "interval",
        "Gateway decision interval in seconds",
        decisionInterval);

    cmd.AddValue(
        "demand",
        "Traffic demand per passenger in Mbps",
        passengerDemandMbps);

    cmd.AddValue(
        "speed",
        "Train speed in m/s",
        trainSpeed);

    cmd.Parse(argc, argv);

    std::cout
        << "\n====================================================\n"
        << " Railway Multi-Connectivity Gateway V5.1\n"
        << " Corrected Traffic Measurement Model\n"
        << " Mobility + Link Quality + Load Balancing\n"
        << "====================================================\n\n";

    // ---------------------------------------------------------
    // NETWORK DEFINITIONS
    // ---------------------------------------------------------

    std::vector<NetworkInfo> networks =
    {
        {30.0, 200.0, 450.0, "Network-1"},
        {20.0, 500.0, 450.0, "Network-2"},
        {15.0, 800.0, 450.0, "Network-3"}
    };

    // ---------------------------------------------------------
    // NODES
    // ---------------------------------------------------------

    NodeContainer gateway;
    gateway.Create(1);

    NodeContainer passengers;
    passengers.Create(passengerCount);

    NodeContainer servers;
    servers.Create(3);

    InternetStackHelper internet;

    internet.Install(gateway);
    internet.Install(passengers);
    internet.Install(servers);

    // ---------------------------------------------------------
    // BACKHAUL LINKS
    // ---------------------------------------------------------

    std::vector<Ipv4Address> serverAddresses;

    for (uint32_t i = 0; i < 3; ++i)
    {
        NodeContainer linkNodes(
            gateway.Get(0),
            servers.Get(i));

        PointToPointHelper p2p;

        p2p.SetDeviceAttribute(
            "DataRate",
            StringValue(
                std::to_string(
                    static_cast<int>(
                        networks[i].capacityMbps)) +
                "Mbps"));

        p2p.SetChannelAttribute(
            "Delay",
            StringValue(
                i == 0 ? "20ms" :
                i == 1 ? "35ms" :
                         "50ms"));

        NetDeviceContainer devices =
            p2p.Install(linkNodes);

        Ipv4AddressHelper address;

        std::string subnet =
            "10.50." +
            std::to_string(i + 1) +
            ".0";

        address.SetBase(
            Ipv4Address(subnet.c_str()),
            "255.255.255.0");

        Ipv4InterfaceContainer interfaces =
            address.Assign(devices);

        serverAddresses.push_back(
            interfaces.GetAddress(1));

        std::cout
            << networks[i].name
            << " | Capacity: "
            << networks[i].capacityMbps
            << " Mbps"
            << " | Base station: "
            << networks[i].basePosition
            << " m\n";
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ---------------------------------------------------------
    // SERVER SINKS
    // ---------------------------------------------------------

    std::vector<ApplicationContainer> sinkApps;

    for (uint32_t i = 0; i < 3; ++i)
    {
        PacketSinkHelper sinkHelper(
            "ns3::UdpSocketFactory",
            InetSocketAddress(
                Ipv4Address::GetAny(),
                9000 + i));

        ApplicationContainer sink =
            sinkHelper.Install(servers.Get(i));

        sink.Start(Seconds(0.0));
        sink.Stop(Seconds(simulationTime + 1.0));

        sinkApps.push_back(sink);
    }

    // ---------------------------------------------------------
    // TRAIN MOBILITY
    // ---------------------------------------------------------

    MobilityHelper trainMobility;

    trainMobility.SetMobilityModel(
        "ns3::ConstantVelocityMobilityModel");

    trainMobility.Install(gateway);

    Ptr<ConstantVelocityMobilityModel> train =
        gateway.Get(0)->GetObject<
            ConstantVelocityMobilityModel>();

    train->SetPosition(
        Vector(0.0, 0.0, 0.0));

    train->SetVelocity(
        Vector(trainSpeed, 0.0, 0.0));

    // ---------------------------------------------------------
    // QUALITY MODEL
    // ---------------------------------------------------------

    auto linkQuality =
        [&](double position, uint32_t link) -> double
    {
        double distance =
            std::abs(
                position -
                networks[link].basePosition);

        if (distance >= networks[link].range)
        {
            return 0.0;
        }

        double factor =
            1.0 -
            distance / networks[link].range;

        return networks[link].capacityMbps *
               std::max(0.0, factor);
    };

    // ---------------------------------------------------------
    // CSV OUTPUT
    // ---------------------------------------------------------

    std::ofstream csv(
        "railway-v5.1-timeseries.csv");

    csv
        << "time,train_position,"
        << "network1_quality,network2_quality,network3_quality,"
        << "network1_passengers,network2_passengers,network3_passengers,"
        << "network1_interval_mbps,network2_interval_mbps,"
        << "network3_interval_mbps,total_interval_mbps\n";

    // ---------------------------------------------------------
    // CONTROLLER + TRAFFIC GENERATOR
    // ---------------------------------------------------------

    for (double t = 1.0;
         t < simulationTime;
         t += decisionInterval)
    {
        Simulator::Schedule(
            Seconds(t),
            [&, t]()
            {
                Vector position =
                    train->GetPosition();

                double trainPosition =
                    position.x;

                std::vector<double> quality(3);

                for (uint32_t n = 0; n < 3; ++n)
                {
                    quality[n] =
                        linkQuality(
                            trainPosition,
                            n);
                }

                std::vector<uint32_t> assigned(3, 0);

                // -------------------------------------------------
                // PASSENGER ASSIGNMENT
                //
                // Each passenger selects a reachable network.
                // The score rewards quality and penalizes load.
                // -------------------------------------------------

                std::vector<uint32_t> passengerLink(
                    passengerCount,
                    0);

                for (uint32_t p = 0;
                     p < passengerCount;
                     ++p)
                {
                    double bestScore = -1.0;
                    uint32_t selected = 0;

                    for (uint32_t n = 0; n < 3; ++n)
                    {
                        if (quality[n] <= 0.0)
                        {
                            continue;
                        }

                        double loadPenalty =
                            1.0 +
                            static_cast<double>(
                                assigned[n]);

                        double score =
                            quality[n] /
                            loadPenalty;

                        if (score > bestScore)
                        {
                            bestScore = score;
                            selected = n;
                        }
                    }

                    passengerLink[p] = selected;
                    assigned[selected]++;
                }

                std::cout
                    << "\n----------------------------------------------------\n";

                std::cout
                    << "Controller t="
                    << std::fixed
                    << std::setprecision(1)
                    << t
                    << " s"
                    << " | Train="
                    << trainPosition
                    << " m\n";

                for (uint32_t n = 0; n < 3; ++n)
                {
                    std::cout
                        << networks[n].name
                        << " | Quality="
                        << std::setprecision(2)
                        << quality[n]
                        << " | Passengers="
                        << assigned[n]
                        << "\n";
                }

                // -------------------------------------------------
                // GENERATE MEASURABLE TRAFFIC
                //
                // The gateway represents the shared passenger
                // access side. Each logical passenger contributes
                // a flow through the selected backhaul.
                // -------------------------------------------------

                for (uint32_t p = 0;
                     p < passengerCount;
                     ++p)
                {
                    uint32_t selected =
                        passengerLink[p];

                    if (quality[selected] <= 0.0)
                    {
                        continue;
                    }

                    OnOffHelper traffic(
                        "ns3::UdpSocketFactory",
                        InetSocketAddress(
                            serverAddresses[selected],
                            9000 + selected));

                    traffic.SetAttribute(
                        "DataRate",
                        StringValue(
                            std::to_string(
                                passengerDemandMbps) +
                            "Mbps"));

                    traffic.SetAttribute(
                        "PacketSize",
                        UintegerValue(1200));

                    double start =
                        t + 0.10 +
                        0.01 * p;

                    double stop =
                        std::min(
                            t + decisionInterval - 0.10,
                            simulationTime - 0.01);

                    if (start >= stop)
                    {
                        continue;
                    }

                    traffic.SetAttribute(
                        "StartTime",
                        TimeValue(
                            Seconds(start)));

                    traffic.SetAttribute(
                        "StopTime",
                        TimeValue(
                            Seconds(stop)));

                    traffic.Install(
                        gateway.Get(0));
                }
            });
    }

    // ---------------------------------------------------------
    // FLOW MONITOR
    // ---------------------------------------------------------

    FlowMonitorHelper flowHelper;

    Ptr<FlowMonitor> monitor =
        flowHelper.InstallAll();

    std::cout
        << "\nStarting V5.1 simulation...\n";

    Simulator::Stop(
        Seconds(simulationTime));

    Simulator::Run();

    // ---------------------------------------------------------
    // FINAL MEASUREMENTS
    // ---------------------------------------------------------

    uint64_t totalBytes = 0;

    std::cout
        << "\n====================================================\n"
        << " V5.1 FINAL RESULTS\n"
        << "====================================================\n";

    for (uint32_t n = 0; n < 3; ++n)
    {
        Ptr<PacketSink> sink =
            DynamicCast<PacketSink>(
                sinkApps[n].Get(0));

        uint64_t bytes =
            sink->GetTotalRx();

        totalBytes += bytes;

        double throughput =
            bytes * 8.0 /
            ((simulationTime - 1.0) *
             1000000.0);

        std::cout
            << networks[n].name
            << " | Received="
            << bytes
            << " bytes"
            << " | Throughput="
            << std::fixed
            << std::setprecision(3)
            << throughput
            << " Mbps\n";
    }

    double aggregate =
        totalBytes * 8.0 /
        ((simulationTime - 1.0) *
         1000000.0);

    std::cout
        << "\nAGGREGATE THROUGHPUT: "
        << aggregate
        << " Mbps\n";

    std::cout
        << "PASSENGERS: "
        << passengerCount
        << "\n";

    std::cout
        << "TRAIN SPEED: "
        << trainSpeed
        << " m/s\n";

    std::cout
        << "ROUTE LENGTH: "
        << trainSpeed * simulationTime
        << " m\n";

    std::cout
        << "CSV: railway-v5.1-timeseries.csv\n";

    std::cout
        << "====================================================\n";

    monitor->SerializeToXmlFile(
        "railway-v5.1-flowmon.xml",
        true,
        true);

    Simulator::Destroy();

    return 0;
}
