#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

int
main(int argc, char* argv[])
{
    uint32_t passengerCount = 12;
    double simulationTime = 20.0;
    double passengerRateMbps = 3.0;

    CommandLine cmd;
    cmd.AddValue("passengers", "Number of passenger devices", passengerCount);
    cmd.AddValue("time", "Simulation time", simulationTime);
    cmd.AddValue("rate", "Traffic rate per passenger in Mbps",
                 passengerRateMbps);
    cmd.Parse(argc, argv);

    std::cout << "\n============================================\n";
    std::cout << " Railway Multi-Connectivity Gateway V2\n";
    std::cout << " Dynamic Link Selection + Load Balancing\n";
    std::cout << "============================================\n\n";

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
    //
    // We use a CSMA LAN here as the simulation equivalent of
    // the passenger Wi-Fi side. This keeps the first gateway
    // experiment focused on routing/load balancing.
    // ---------------------------------------------------------
    NodeContainer passengerLan;
    passengerLan.Add(gateway);

    for (uint32_t i = 0; i < passengerCount; ++i)
    {
        passengerLan.Add(passengers.Get(i));
    }

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(10)));

    NetDeviceContainer lanDevices =
        csma.Install(passengerLan);

    Ipv4AddressHelper lanAddress;
    lanAddress.SetBase(
        "192.168.1.0",
        "255.255.255.0");

    lanAddress.Assign(lanDevices);

    // ---------------------------------------------------------
    // THREE INTERNET LINKS
    // ---------------------------------------------------------
    std::vector<double> capacities = {
        30.0,
        20.0,
        15.0
    };

    std::vector<std::string> rates = {
        "30Mbps",
        "20Mbps",
        "15Mbps"
    };

    std::vector<std::string> delays = {
        "20ms",
        "35ms",
        "50ms"
    };

    std::vector<Ipv4Address> serverAddresses;

    for (uint32_t i = 0; i < 3; ++i)
    {
        NodeContainer linkNodes(
            gateway.Get(0),
            servers.Get(i));

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute(
            "DataRate",
            StringValue(rates[i]));

        p2p.SetChannelAttribute(
            "Delay",
            StringValue(delays[i]));

        NetDeviceContainer devices =
            p2p.Install(linkNodes);

        Ipv4AddressHelper address;

        std::string subnet =
            "10.10." + std::to_string(i + 1) + ".0";

        address.SetBase(
            Ipv4Address(subnet.c_str()),
            "255.255.255.0");

        Ipv4InterfaceContainer interfaces =
            address.Assign(devices);

        serverAddresses.push_back(
            interfaces.GetAddress(1));

        std::cout
            << "Link " << i + 1
            << "  Capacity: " << capacities[i]
            << " Mbps  Delay: " << delays[i]
            << "\n";
    }

    // ---------------------------------------------------------
    // ROUTING
    // ---------------------------------------------------------
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
    // DYNAMIC LOAD BALANCER
    //
    // Choose the link with the lowest current load/capacity.
    // Higher-capacity links naturally receive more users.
    // ---------------------------------------------------------
    std::vector<uint32_t> assigned(3, 0);

    for (uint32_t i = 0; i < passengerCount; ++i)
    {
        double bestScore = 1e9;
        uint32_t bestLink = 0;

        for (uint32_t link = 0; link < 3; ++link)
        {
            double score =
                static_cast<double>(assigned[link]) /
                capacities[link];

            if (score < bestScore)
            {
                bestScore = score;
                bestLink = link;
            }
        }

        assigned[bestLink]++;

        uint16_t port = 9000 + bestLink;

        OnOffHelper traffic(
            "ns3::UdpSocketFactory",
            InetSocketAddress(
                serverAddresses[bestLink],
                port));

        std::string rate =
            std::to_string(passengerRateMbps) + "Mbps";

        traffic.SetAttribute(
            "DataRate",
            StringValue(rate));

        traffic.SetAttribute(
            "PacketSize",
            UintegerValue(1200));

        traffic.SetAttribute(
            "StartTime",
            TimeValue(
                Seconds(2.0 + 0.05 * i)));

        traffic.SetAttribute(
            "StopTime",
            TimeValue(
                Seconds(simulationTime - 1)));

        traffic.Install(passengers.Get(i));

        std::cout
            << "Passenger " << i + 1
            << " -> Link " << bestLink + 1
            << "\n";
    }

    std::cout << "\nInitial load distribution:\n";

    for (uint32_t i = 0; i < 3; ++i)
    {
        std::cout
            << "Link " << i + 1
            << " -> "
            << assigned[i]
            << " passengers\n";
    }

    // ---------------------------------------------------------
    // FLOW MONITOR
    // ---------------------------------------------------------
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor =
        flowHelper.InstallAll();

    std::cout << "\nStarting V2 simulation...\n\n";

    Simulator::Stop(
        Seconds(simulationTime));

    Simulator::Run();

    // ---------------------------------------------------------
    // RESULTS
    // ---------------------------------------------------------
    uint64_t totalBytes = 0;

    std::cout
        << "----------- V2 LINK RESULTS -----------\n";

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
            ((simulationTime - 3.0) * 1000000.0);

        std::cout
            << std::fixed
            << std::setprecision(3)
            << "Link " << i + 1
            << " | Capacity: "
            << capacities[i]
            << " Mbps"
            << " | Throughput: "
            << throughput
            << " Mbps"
            << " | Passengers: "
            << assigned[i]
            << "\n";
    }

    double aggregateThroughput =
        (totalBytes * 8.0) /
        ((simulationTime - 3.0) * 1000000.0);

    std::cout
        << "\n----------------------------------------\n";

    std::cout
        << "AGGREGATE THROUGHPUT: "
        << aggregateThroughput
        << " Mbps\n";

    std::cout
        << "TOTAL PASSENGERS: "
        << passengerCount
        << "\n";

    std::cout
        << "----------------------------------------\n\n";

    monitor->SerializeToXmlFile(
        "railway-v2-flowmon.xml",
        true,
        true);

    Simulator::Destroy();

    return 0;
}
