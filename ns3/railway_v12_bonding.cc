#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <vector>

using namespace ns3;

/**
 * V12 packet-level bonding prototype.
 *
 * This is deliberately a small, self-contained ns-3 demonstration of the
 * mechanisms needed for multi-link bonding:
 *   - packet sequence numbers
 *   - weighted striping across multiple links
 *   - per-path forwarding
 *   - receiver-side in-order reassembly
 *   - duplicate/out-of-order accounting
 *
 * It is a research prototype, not a replacement for production MPTCP,
 * MPQUIC, or SD-WAN implementations.
 */

class BondingReceiver : public Application
{
  public:
    BondingReceiver() = default;
    ~BondingReceiver() override = default;

    uint64_t GetDeliveredBytes() const { return m_deliveredBytes; }
    uint64_t GetOutOfOrderPackets() const { return m_outOfOrderPackets; }
    uint64_t GetDuplicatePackets() const { return m_duplicatePackets; }

  protected:
    void StartApplication() override
    {
        m_socket = Socket::CreateSocket(
            GetNode(),
            UdpSocketFactory::GetTypeId());

        InetSocketAddress local(
            Ipv4Address::GetAny(),
            9000);

        m_socket->Bind(local);
        m_socket->SetRecvCallback(
            MakeCallback(&BondingReceiver::ReceivePacket, this));
    }

    void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }

  private:
    void ReceivePacket(Ptr<Socket> socket)
    {
        Address from;
        Ptr<Packet> packet;

        while ((packet = socket->RecvFrom(from)))
        {
            if (packet->GetSize() < 12)
            {
                continue;
            }

            uint8_t buffer[12];
            packet->CopyData(buffer, sizeof(buffer));

            uint64_t sequence = 0;
            for (int i = 0; i < 8; ++i)
            {
                sequence = (sequence << 8) | buffer[i];
            }

            uint32_t payloadBytes = 0;
            for (int i = 8; i < 12; ++i)
            {
                payloadBytes =
                    (payloadBytes << 8) | buffer[i];
            }

            if (m_seen.find(sequence) != m_seen.end())
            {
                ++m_duplicatePackets;
                continue;
            }

            m_seen.insert(sequence);

            if (sequence > m_nextExpected)
            {
                ++m_outOfOrderPackets;
            }

            if (sequence == m_nextExpected)
            {
                m_deliveredBytes += payloadBytes;
                ++m_nextExpected;

                // Drain contiguous packets that arrived out of order.
                while (true)
                {
                    auto it = m_pending.find(m_nextExpected);
                    if (it == m_pending.end())
                    {
                        break;
                    }

                    m_deliveredBytes += it->second;
                    m_pending.erase(it);
                    ++m_nextExpected;
                }
            }
            else if (sequence > m_nextExpected)
            {
                m_pending.emplace(sequence, payloadBytes);
            }
        }
    }

    Ptr<Socket> m_socket;
    uint64_t m_nextExpected{0};
    uint64_t m_deliveredBytes{0};
    uint64_t m_outOfOrderPackets{0};
    uint64_t m_duplicatePackets{0};
    std::set<uint64_t> m_seen;
    std::map<uint64_t, uint32_t> m_pending;
};

class BondingSender : public Application
{
  public:
    BondingSender() = default;
    ~BondingSender() override = default;

    void Configure(const std::vector<Address>& destinations,
                   const std::vector<uint32_t>& weights,
                   uint32_t packetSize,
                   uint32_t packetsPerSecond)
    {
        m_destinations = destinations;
        m_weights = weights;
        m_packetSize = packetSize;
        m_packetsPerSecond = packetsPerSecond;
    }

    uint64_t GetSentPackets() const { return m_sentPackets; }
    uint64_t GetBytes() const { return m_bytes; }

  protected:
    void StartApplication() override
    {
        for (const auto& destination : m_destinations)
        {
            Ptr<Socket> socket = Socket::CreateSocket(
                GetNode(),
                UdpSocketFactory::GetTypeId());
            socket->Connect(destination);
            m_sockets.push_back(socket);
        }

        m_event = Simulator::Schedule(
            Seconds(0.0),
            &BondingSender::SendOne,
            this);
    }

    void StopApplication() override
    {
        if (m_event.IsRunning())
        {
            Simulator::Cancel(m_event);
        }

        for (auto& socket : m_sockets)
        {
            socket->Close();
        }
    }

  private:
    void SendOne()
    {
        if (m_sockets.empty())
        {
            return;
        }

        uint32_t selected = WeightedSelect();
        Ptr<Packet> packet = CreateBondedPacket(m_nextSequence, m_packetSize);
        m_sockets[selected]->Send(packet);

        ++m_nextSequence;
        ++m_sentPackets;
        m_bytes += m_packetSize;

        const double period =
            1.0 / static_cast<double>(std::max(1u, m_packetsPerSecond));

        m_event = Simulator::Schedule(
            Seconds(period),
            &BondingSender::SendOne,
            this);
    }

    uint32_t WeightedSelect()
    {
        uint32_t total = 0;
        for (auto weight : m_weights)
        {
            total += std::max(1u, weight);
        }

        const uint32_t ticket = m_nextSequence % total;
        uint32_t cumulative = 0;
        for (uint32_t i = 0; i < m_weights.size(); ++i)
        {
            cumulative += std::max(1u, m_weights[i]);
            if (ticket < cumulative)
            {
                return i;
            }
        }

        return 0;
    }

    Ptr<Packet> CreateBondedPacket(uint64_t sequence, uint32_t payloadBytes)
    {
        std::vector<uint8_t> bytes(12 + payloadBytes, 0);

        for (int i = 7; i >= 0; --i)
        {
            bytes[i] = static_cast<uint8_t>(sequence & 0xff);
            sequence >>= 8;
        }

        uint32_t size = payloadBytes;
        for (int i = 11; i >= 8; --i)
        {
            bytes[i] = static_cast<uint8_t>(size & 0xff);
            size >>= 8;
        }

        return Create<Packet>(bytes.data(), bytes.size());
    }

    std::vector<Address> m_destinations;
    std::vector<uint32_t> m_weights;
    std::vector<Ptr<Socket>> m_sockets;
    EventId m_event;
    uint64_t m_nextSequence{0};
    uint64_t m_sentPackets{0};
    uint64_t m_bytes{0};
    uint32_t m_packetSize{1200};
    uint32_t m_packetsPerSecond{1000};
};

int
main(int argc, char* argv[])
{
    double simulationTime = 20.0;
    uint32_t packetSize = 1200;
    uint32_t packetsPerSecond = 1000;

    CommandLine cmd;
    cmd.AddValue("time", "Simulation time in seconds", simulationTime);
    cmd.AddValue("packetSize", "Bonded payload size", packetSize);
    cmd.AddValue("pps", "Packets per second", packetsPerSecond);
    cmd.Parse(argc, argv);

    std::cout << "\n=================================================\n";
    std::cout << " Railway Gateway V12\n";
    std::cout << " Packet-Level Multi-Link Bonding Prototype\n";
    std::cout << "=================================================\n\n";

    NodeContainer gateway;
    gateway.Create(1);

    NodeContainer servers;
    servers.Create(3);

    InternetStackHelper internet;
    internet.Install(gateway);
    internet.Install(servers);

    const std::vector<std::string> rates = {"30Mbps", "20Mbps", "15Mbps"};
    const std::vector<std::string> delays = {"20ms", "35ms", "50ms"};

    std::vector<Ipv4Address> serverAddresses;

    for (uint32_t i = 0; i < 3; ++i)
    {
        NodeContainer pair(gateway.Get(0), servers.Get(i));

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(rates[i]));
        p2p.SetChannelAttribute("Delay", StringValue(delays[i]));

        NetDeviceContainer devices = p2p.Install(pair);

        Ipv4AddressHelper address;
        address.SetBase(
            Ipv4Address(("10.70." + std::to_string(i + 1) + ".0").c_str()),
            "255.255.255.0");

        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        serverAddresses.push_back(interfaces.GetAddress(1));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ptr<BondingReceiver> receiver = CreateObject<BondingReceiver>();
    gateway.Get(0)->AddApplication(receiver);
    receiver->SetStartTime(Seconds(0.0));
    receiver->SetStopTime(Seconds(simulationTime));

    std::vector<Address> destinations;
    for (auto address : serverAddresses)
    {
        destinations.emplace_back(
            InetSocketAddress(address, 9000));
    }

    // Nominal weights approximate the relative path capacities.
    std::vector<uint32_t> weights = {6, 4, 3};

    Ptr<BondingSender> sender = CreateObject<BondingSender>();
    sender->Configure(
        destinations,
        weights,
        packetSize,
        packetsPerSecond);
    gateway.Get(0)->AddApplication(sender);
    sender->SetStartTime(Seconds(1.0));
    sender->SetStopTime(Seconds(simulationTime - 1.0));

    std::cout << "Link weights: 6 / 4 / 3\n";
    std::cout << "Packet size : " << packetSize << " bytes\n";
    std::cout << "Packet rate : " << packetsPerSecond << " packets/s\n";
    std::cout << "Starting simulation...\n";

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    const double measuredTime =
        std::max(0.001, simulationTime - 2.0);

    const double goodputMbps =
        (receiver->GetDeliveredBytes() * 8.0) /
        (measuredTime * 1000000.0);

    std::cout << "\n---------------- V12 RESULTS ----------------\n";
    std::cout << "Sent packets       : " << sender->GetSentPackets() << "\n";
    std::cout << "Sent bytes         : " << sender->GetBytes() << "\n";
    std::cout << "In-order bytes     : " << receiver->GetDeliveredBytes() << "\n";
    std::cout << "Out-of-order pkts  : " << receiver->GetOutOfOrderPackets() << "\n";
    std::cout << "Duplicate packets  : " << receiver->GetDuplicatePackets() << "\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Effective goodput  : " << goodputMbps << " Mbps\n";
    std::cout << "-------------------------------------------------\n";
    std::cout << "NOTE: prototype packet striping/reassembly model;\n";
    std::cout << "not production-grade MPTCP/MPQUIC/SD-WAN bonding.\n";
    std::cout << "=================================================\n";

    Simulator::Destroy();
    return 0;
}
