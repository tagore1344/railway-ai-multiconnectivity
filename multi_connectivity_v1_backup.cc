#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"

#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char* argv[])
{
    uint32_t passengerCount = 6;
    double simulationTime = 20.0;

    CommandLine cmd;
    cmd.AddValue("passengers", "Number of passenger devices", passengerCount);
    cmd.AddValue("time", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    std::cout << "\n========================================\n";
    std::cout << " Railway Multi-Connectivity Gateway V1\n";
    std::cout << "========================================\n\n";

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

    std::vector<std::string> dataRates = {
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
        NodeContainer linkNodes(gateway.Get(0), servers.Get(i));

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(dataRates[i]));
        p2p.SetChannelAttribute("Delay", StringValue(delays[i]));

        NetDeviceContainer devices = p2p.Install(linkNodes);

        Ipv4AddressHelper address;
        address.SetBase(
            Ipv4Address(("10.1." + std::to_string(i + 1) + ".0").c_str()),
            "255.255.255.0");

        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        serverAddresses.push_back(interfaces.GetAddress(1));

        std::cout << "Internet Link " << i + 1
                  << " : " << dataRates[i]
                  << " , " << delays[i] << "\n";
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("Railway-Gateway");

    mac.SetType(
        "ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));

    NetDeviceContainer passengerDevices =
        wifi.Install(phy, mac, passengers);

    mac.SetType(
        "ns3::ApWifiMac",
        "Ssid", SsidValue(ssid));

    NetDeviceContainer gatewayWifi =
        wifi.Install(phy, mac, gateway);

    MobilityHelper mobility;

    mobility.SetPositionAllocator(
        "ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(2.0),
        "DeltaY", DoubleValue(2.0),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel");

    mobility.Install(passengers);
    mobility.Install(gateway);
    mobility.Install(servers);

    Ipv4AddressHelper wifiAddress;
    wifiAddress.SetBase(
        "192.168.1.0",
        "255.255.255.0");

    wifiAddress.Assign(passengerDevices);
    wifiAddress.Assign(gatewayWifi);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

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
        sink.Stop(Seconds(simulationTime));

        sinkApps.push_back(sink);
    }

    for (uint32_t i = 0; i < passengerCount; ++i)
    {
        uint32_t link = i % 3;
        uint16_t port = 9000 + link;

        OnOffHelper client(
            "ns3::UdpSocketFactory",
            InetSocketAddress(
                serverAddresses[link],
                port));

        client.SetAttribute(
            "DataRate",
            StringValue("8Mbps"));

        client.SetAttribute(
            "PacketSize",
            UintegerValue(1200));

        client.SetAttribute(
            "StartTime",
            TimeValue(Seconds(2.0)));

        client.SetAttribute(
            "StopTime",
            TimeValue(Seconds(simulationTime - 1)));

        client.Install(passengers.Get(i));
    }

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    std::cout << "\nStarting simulation...\n\n";

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint64_t totalReceived = 0;

    std::cout << "----------- LINK RESULTS -----------\n";

    for (uint32_t i = 0; i < 3; ++i)
    {
        Ptr<PacketSink> sink =
            DynamicCast<PacketSink>(sinkApps[i].Get(0));

        uint64_t bytes = sink->GetTotalRx();
        totalReceived += bytes;

        double throughput =
            (bytes * 8.0) /
            ((simulationTime - 3.0) * 1000000.0);

        std::cout << "Link " << i + 1
                  << " received: " << bytes
                  << " bytes"
                  << " | Throughput: " << throughput
                  << " Mbps\n";
    }

    double totalThroughput =
        (totalReceived * 8.0) /
        ((simulationTime - 3.0) * 1000000.0);

    std::cout << "\n------------------------------------\n";
    std::cout << "TOTAL EFFECTIVE THROUGHPUT: "
              << totalThroughput << " Mbps\n";
    std::cout << "PASSENGERS: "
              << passengerCount << "\n";
    std::cout << "SIMULATION TIME: "
              << simulationTime << " seconds\n";
    std::cout << "------------------------------------\n\n";

    monitor->SerializeToXmlFile(
        "railway-multi-connectivity-flowmon.xml",
        true,
        true);

    Simulator::Destroy();

    return 0;
}
