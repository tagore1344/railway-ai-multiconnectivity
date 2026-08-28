#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace ns3;

struct LinkInfo
{
    double nominalCapacity;
    double baseX;
    double maxRange;
    std::string name;
};

int
main(int argc, char* argv[])
{
    uint32_t passengerCount = 12;
    double simulationTime = 60.0;
    double passengerRateMbps = 2.0;
    double controlInterval = 10.0;
    double trainSpeed = 20.0;

    CommandLine cmd;

    cmd.AddValue(
        "passengers",
        "Number of passengers",
        passengerCount);

    cmd.AddValue(
        "time",
        "Simulation time in seconds",
        simulationTime);

    cmd.AddValue(
        "rate",
        "Passenger traffic rate in Mbps",
        passengerRateMbps);

    cmd.Parse(argc, argv);

    std::cout << "\n===============================================\n";
    std::cout << " Railway Multi-Connectivity Gateway V4\n";
    std::cout << " Train Mobility + Dynamic Link Selection\n";
    std::cout << "===============================================\n\n";

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
    // PASSENGER ACCESS NETWORK
    // ---------------------------------------------------------

    NodeContainer passengerLan;

    passengerLan.Add(gateway);

    for (uint32_t i = 0; i < passengerCount; ++i)
    {
        passengerLan.Add(passengers.Get(i));
    }

    CsmaHelper csma;

    csma.SetChannelAttribute(
        "DataRate",
        StringValue("1Gbps"));

    csma.SetChannelAttribute(
        "Delay",
        TimeValue(NanoSeconds(10)));

    NetDeviceContainer lanDevices =
        csma.Install(passengerLan);

    Ipv4AddressHelper lanAddress;

    lanAddress.SetBase(
        "192.168.10.0",
        "255.255.255.0");

    lanAddress.Assign(lanDevices);

    // ---------------------------------------------------------
    // THREE CELLULAR/BACKHAUL LINKS
    // ---------------------------------------------------------

    std::vector<LinkInfo> links =
    {
        {30.0, 200.0, 450.0, "Network-1"},
        {20.0, 500.0, 450.0, "Network-2"},
        {15.0, 800.0, 450.0, "Network-3"}
    };

    std::vector<Ipv4Address> serverAddresses;

    for (uint32_t i = 0; i < links.size(); ++i)
    {
        NodeContainer linkNodes(
            gateway.Get(0),
            servers.Get(i));

        PointToPointHelper p2p;

        std::string rate =
            std::to_string(
                static_cast<int>(links[i].nominalCapacity))
            + "Mbps";

        p2p.SetDeviceAttribute(
            "DataRate",
            StringValue(rate));

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
            "10.30." +
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
            << links[i].name
            << " | Nominal capacity: "
            << links[i].nominalCapacity
            << " Mbps"
            << " | Base station position: "
            << links[i].baseX
            << " m\n";
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ---------------------------------------------------------
    // SERVER SINKS
    // ---------------------------------------------------------

    std::vector<ApplicationContainer> sinks;

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
        sink.Stop(Seconds(simulationTime));

        sinks.push_back(sink);
    }

    // ---------------------------------------------------------
    // TRAIN MOBILITY
    // ---------------------------------------------------------

    MobilityHelper trainMobility;

    trainMobility.SetMobilityModel(
        "ns3::ConstantVelocityMobilityModel");

    trainMobility.Install(gateway);

    Ptr<ConstantVelocityMobilityModel> trainModel =
        gateway.Get(0)->GetObject<
            ConstantVelocityMobilityModel>();

    trainModel->SetPosition(
        Vector(0.0, 0.0, 0.0));

    trainModel->SetVelocity(
        Vector(trainSpeed, 0.0, 0.0));

    // ---------------------------------------------------------
    // PASSENGER POSITIONS
    // ---------------------------------------------------------

    MobilityHelper passengerMobility;

    passengerMobility.SetPositionAllocator(
        "ns3::GridPositionAllocator",
        "MinX",
        DoubleValue(1.0),
        "MinY",
        DoubleValue(0.0),
        "DeltaX",
        DoubleValue(2.0),
        "DeltaY",
        DoubleValue(2.0),
        "GridWidth",
        UintegerValue(4),
        "LayoutType",
        StringValue("RowFirst"));

    passengerMobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel");

    passengerMobility.Install(passengers);

    // ---------------------------------------------------------
    // LINK QUALITY MODEL
    //
    // Quality depends on distance between the train and
    // the simulated cellular base station.
    // ---------------------------------------------------------

    auto calculateQuality =
        [&](double trainX, uint32_t link) -> double
    {
        double distance =
            std::abs(trainX - links[link].baseX);

        if (distance >= links[link].maxRange)
        {
            return 0.0;
        }

        double distanceFactor =
            1.0 -
            (distance / links[link].maxRange);

        if (distanceFactor < 0.0)
        {
            distanceFactor = 0.0;
        }

        // Capacity-weighted quality score.
        return
            links[link].nominalCapacity *
            distanceFactor;
    };

    // ---------------------------------------------------------
    // DYNAMIC CONTROLLER
    //
    // Every controlInterval seconds, the gateway examines
    // its current position and redistributes passengers.
    // ---------------------------------------------------------

    for (double t = 2.0;
         t < simulationTime - 1.0;
         t += controlInterval)
    {
        uint32_t intervalIndex =
            static_cast<uint32_t>(
                (t - 2.0) / controlInterval);

        Simulator::Schedule(
            Seconds(t),
            [&, t, intervalIndex]()
            {
                Vector position =
                    trainModel->GetPosition();

                double trainX =
                    position.x;

                std::vector<double> quality(3);

                for (uint32_t link = 0;
                     link < 3;
                     ++link)
                {
                    quality[link] =
                        calculateQuality(
                            trainX,
                            link);
                }

                // Current normalized load.
                std::vector<uint32_t> load(
                    3,
                    0);

                // Passenger assignment for this interval.
                std::vector<uint32_t> assignment(
                    passengerCount,
                    0);

                for (uint32_t p = 0;
                     p < passengerCount;
                     ++p)
                {
                    double bestScore =
                        -1.0;

                    uint32_t selectedLink =
                        0;

                    for (uint32_t link = 0;
                         link < 3;
                         ++link)
                    {
                        if (quality[link] <= 0.0)
                        {
                            continue;
                        }

                        double loadFactor =
                            static_cast<double>(
                                load[link]) /
                            std::max(
                                quality[link],
                                0.1);

                        double score =
                            1.0 /
                            (1.0 + loadFactor);

                        score *=
                            quality[link];

                        if (score > bestScore)
                        {
                            bestScore = score;
                            selectedLink =
                                link;
                        }
                    }

                    assignment[p] =
                        selectedLink;

                    load[selectedLink]++;
                }

                std::cout
                    << "\n========== CONTROLLER t="
                    << std::fixed
                    << std::setprecision(1)
                    << t
                    << " s ==========\n";

                std::cout
                    << "Train position: "
                    << trainX
                    << " m\n";

                for (uint32_t link = 0;
                     link < 3;
                     ++link)
                {
                    std::cout
                        << links[link].name
                        << " | quality score: "
                        << std::setprecision(2)
                        << quality[link]
                        << " | passengers: "
                        << load[link]
                        << "\n";
                }

                for (uint32_t p = 0;
                     p < passengerCount;
                     ++p)
                {
                    uint32_t link =
                        assignment[p];

                    double start =
                        t + 0.01 *
                        static_cast<double>(p);

                    double stop =
                        std::min(
                            t + controlInterval,
                            simulationTime - 1.0);

                    if (start >= stop)
                    {
                        continue;
                    }

                    OnOffHelper traffic(
                        "ns3::UdpSocketFactory",
                        InetSocketAddress(
                            serverAddresses[link],
                            9000 + link));

                    traffic.SetAttribute(
                        "DataRate",
                        StringValue(
                            std::to_string(
                                passengerRateMbps) +
                            "Mbps"));

                    traffic.SetAttribute(
                        "PacketSize",
                        UintegerValue(1200));

                    traffic.SetAttribute(
                        "StartTime",
                        TimeValue(
                            Seconds(start)));

                    traffic.SetAttribute(
                        "StopTime",
                        TimeValue(
                            Seconds(stop)));

                    traffic.Install(
                        passengers.Get(p));
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
        << "\nStarting V4 simulation...\n";

    Simulator::Stop(
        Seconds(simulationTime));

    Simulator::Run();

    // ---------------------------------------------------------
    // FINAL RESULTS
    // ---------------------------------------------------------

    uint64_t totalBytes = 0;

    std::cout
        << "\n===============================================\n";
    std::cout
        << " V4 FINAL RESULTS\n";
    std::cout
        << "===============================================\n";

    for (uint32_t i = 0; i < 3; ++i)
    {
        Ptr<PacketSink> sink =
            DynamicCast<PacketSink>(
                sinks[i].Get(0));

        uint64_t bytes =
            sink->GetTotalRx();

        totalBytes += bytes;

        double throughput =
            (bytes * 8.0) /
            ((simulationTime - 2.0) *
             1000000.0);

        std::cout
            << links[i].name
            << " | Received: "
            << bytes
            << " bytes"
            << " | Throughput: "
            << std::fixed
            << std::setprecision(3)
            << throughput
            << " Mbps\n";
    }

    double aggregate =
        (totalBytes * 8.0) /
        ((simulationTime - 2.0) *
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
        << "ROUTE LENGTH APPROX.: "
        << trainSpeed * simulationTime
        << " m\n";

    std::cout
        << "===============================================\n";

    monitor->SerializeToXmlFile(
        "railway-v4-flowmon.xml",
        true,
        true);

    Simulator::Destroy();

    return 0;
}
