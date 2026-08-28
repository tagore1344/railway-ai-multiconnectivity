#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace ns3;

struct Network
{
    double capacity;
    double basePosition;
    double range;
    std::string name;
};

int
main(int argc, char* argv[])
{
    uint32_t passengersCount = 12;
    double simulationTime = 60.0;
    double interval = 5.0;
    double trainSpeed = 20.0;

    CommandLine cmd;
    cmd.AddValue(
        "passengers",
        "Number of passenger devices",
        passengersCount);

    cmd.AddValue(
        "time",
        "Simulation duration",
        simulationTime);

    cmd.AddValue(
        "interval",
        "Gateway decision interval",
        interval);

    cmd.AddValue(
        "speed",
        "Train speed in m/s",
        trainSpeed);

    cmd.Parse(argc, argv);

    std::cout
        << "\n=================================================\n"
        << " Railway Multi-Connectivity Gateway V5\n"
        << " Mobility + Quality Monitoring + Load Balancing\n"
        << "=================================================\n\n";

    // ---------------------------------------------------------
    // NETWORK DEFINITIONS
    // ---------------------------------------------------------

    std::vector<Network> networks =
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
    passengers.Create(passengersCount);

    NodeContainer servers;
    servers.Create(3);

    InternetStackHelper internet;

    internet.Install(gateway);
    internet.Install(passengers);
    internet.Install(servers);

    // ---------------------------------------------------------
    // PASSENGER LAN
    // ---------------------------------------------------------

    NodeContainer passengerLan;

    passengerLan.Add(gateway);

    for (uint32_t i = 0; i < passengersCount; ++i)
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
        "192.168.20.0",
        "255.255.255.0");

    lanAddress.Assign(lanDevices);

    // ---------------------------------------------------------
    // THREE BACKHAUL LINKS
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
                        networks[i].capacity)) +
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
            "10.40." +
            std::to_string(i + 1) +
            ".0";

        address.SetBase(
            Ipv4Address(subnet.c_str()),
            "255.255.255.0");

        Ipv4InterfaceContainer interfaces =
            address.Assign(devices);

        serverAddresses.push_back(
            interfaces.GetAddress(1));
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

    Ptr<ConstantVelocityMobilityModel> train =
        gateway.Get(0)->GetObject<
            ConstantVelocityMobilityModel>();

    train->SetPosition(
        Vector(0.0, 0.0, 0.0));

    train->SetVelocity(
        Vector(trainSpeed, 0.0, 0.0));

    // ---------------------------------------------------------
    // PASSENGER POSITIONS
    // ---------------------------------------------------------

    MobilityHelper passengerMobility;

    passengerMobility.SetPositionAllocator(
        "ns3::GridPositionAllocator",
        "MinX",
        DoubleValue(2.0),
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
    // CSV LOG
    // ---------------------------------------------------------

    std::ofstream csv(
        "railway-v5-controller.csv");

    csv << "time,train_position,"
        << "network1_quality,network2_quality,network3_quality,"
        << "network1_passengers,network2_passengers,network3_passengers\n";

    // ---------------------------------------------------------
    // CONTROLLER
    // ---------------------------------------------------------

    auto quality =
        [&](double trainPosition,
            uint32_t network) -> double
    {
        double distance =
            std::abs(
                trainPosition -
                networks[network].basePosition);

        if (distance >= networks[network].range)
        {
            return 0.0;
        }

        double factor =
            1.0 -
            distance / networks[network].range;

        return
            networks[network].capacity *
            std::max(0.0, factor);
    };

    for (double t = 2.0;
         t < simulationTime;
         t += interval)
    {
        Simulator::Schedule(
            Seconds(t),
            [&, t]()
            {
                Vector position =
                    train->GetPosition();

                double trainPosition =
                    position.x;

                std::vector<double> q(3);

                for (uint32_t n = 0; n < 3; ++n)
                {
                    q[n] =
                        quality(
                            trainPosition,
                            n);
                }

                std::vector<uint32_t> load(
                    3,
                    0);

                // Assign each passenger to the network
                // having the best quality/load score.
                for (uint32_t p = 0;
                     p < passengersCount;
                     ++p)
                {
                    double bestScore = -1.0;
                    uint32_t selected = 0;

                    for (uint32_t n = 0;
                         n < 3;
                         ++n)
                    {
                        if (q[n] <= 0.0)
                        {
                            continue;
                        }

                        double normalizedLoad =
                            static_cast<double>(
                                load[n]) /
                            std::max(q[n], 0.1);

                        double score =
                            q[n] /
                            (1.0 +
                             normalizedLoad);

                        if (score > bestScore)
                        {
                            bestScore = score;
                            selected = n;
                        }
                    }

                    load[selected]++;
                }

                std::cout
                    << "\n-------------------------------------------------\n";

                std::cout
                    << "Controller time: "
                    << std::fixed
                    << std::setprecision(1)
                    << t
                    << " s\n";

                std::cout
                    << "Train position: "
                    << trainPosition
                    << " m\n";

                for (uint32_t n = 0;
                     n < 3;
                     ++n)
                {
                    std::cout
                        << networks[n].name
                        << " | Quality: "
                        << std::setprecision(2)
                        << q[n]
                        << " | Passengers: "
                        << load[n]
                        << "\n";
                }

                csv
                    << t << ","
                    << trainPosition << ","
                    << q[0] << ","
                    << q[1] << ","
                    << q[2] << ","
                    << load[0] << ","
                    << load[1] << ","
                    << load[2] << "\n";
            });
    }

    // ---------------------------------------------------------
    // FLOW MONITOR
    // ---------------------------------------------------------

    FlowMonitorHelper flowHelper;

    Ptr<FlowMonitor> monitor =
        flowHelper.InstallAll();

    std::cout
        << "\nStarting V5 simulation...\n";

    Simulator::Stop(
        Seconds(simulationTime));

    Simulator::Run();

    csv.close();

    // ---------------------------------------------------------
    // RESULTS
    // ---------------------------------------------------------

    uint64_t totalBytes = 0;

    std::cout
        << "\n=================================================\n"
        << " V5 FINAL RESULTS\n"
        << "=================================================\n";

    for (uint32_t n = 0; n < 3; ++n)
    {
        Ptr<PacketSink> sink =
            DynamicCast<PacketSink>(
                sinks[n].Get(0));

        uint64_t received =
            sink->GetTotalRx();

        totalBytes += received;

        double throughput =
            received *
            8.0 /
            ((simulationTime - 2.0) *
             1000000.0);

        std::cout
            << networks[n].name
            << " | Received: "
            << received
            << " bytes"
            << " | Measured throughput: "
            << std::fixed
            << std::setprecision(3)
            << throughput
            << " Mbps\n";
    }

    double aggregate =
        totalBytes *
        8.0 /
        ((simulationTime - 2.0) *
         1000000.0);

    std::cout
        << "\nAGGREGATE THROUGHPUT: "
        << aggregate
        << " Mbps\n";

    std::cout
        << "PASSENGERS: "
        << passengersCount
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
        << "\nController log saved to:\n"
        << "railway-v5-controller.csv\n";

    std::cout
        << "=================================================\n";

    monitor->SerializeToXmlFile(
        "railway-v5-flowmon.xml",
        true,
        true);

    Simulator::Destroy();

    return 0;
}
