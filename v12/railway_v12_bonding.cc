#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RailwayV12PacketBonding");

namespace
{

class BondingHeader : public Header
{
  public:
    BondingHeader() = default;

    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("BondingHeader")
                .SetParent<Header>()
                .AddConstructor<BondingHeader>();
        return tid;
    }

    TypeId GetInstanceTypeId() const override { return GetTypeId(); }

    void Set(uint64_t sequence, uint32_t payloadSize)
    {
        m_sequence = sequence;
        m_payloadSize = payloadSize;
    }

    uint64_t GetSequence() const { return m_sequence; }
    uint32_t GetPayloadSize() const { return m_payloadSize; }

    uint32_t GetSerializedSize() const override { return 12; }

    void Serialize(Buffer::Iterator start) const override
    {
        start.WriteHtonU64(m_sequence);
        start.WriteHtonU32(m_payloadSize);
    }

    uint32_t Deserialize(Buffer::Iterator start) override
    {
        m_sequence = start.ReadNtohU64();
        m_payloadSize = start.ReadNtohU32();
        return GetSerializedSize();
    }

    void Print(std::ostream& os) const override
    {
        os << "seq=" << m_sequence << " payload=" << m_payloadSize;
    }

  private:
    uint64_t m_sequence = 0;
    uint32_t m_payloadSize = 0;
};

class BondingReceiver : public Application
{
  public:
    void AddSocket(Ptr<Socket> socket)
    {
        m_sockets.push_back(socket);
        socket->SetRecvCallback(MakeCallback(&BondingReceiver::HandleRead, this));
    }

    uint64_t GetDeliveredPackets() const { return m_deliveredPackets; }
    uint64_t GetReceivedPackets() const { return m_receivedPackets; }
    uint64_t GetDroppedPackets() const { return m_droppedPackets; }
    uint64_t GetReorderedPackets() const { return m_reorderedPackets; }
    uint64_t GetDeliveredBytes() const { return m_deliveredBytes; }

  private:
    void StartApplication() override {}

    void StopApplication() override
    {
        for (auto socket : m_sockets)
        {
            if (socket)
            {
                socket->Close();
            }
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        while (auto packet = socket->Recv())
        {
            ++m_receivedPackets;

            BondingHeader header;
            if (packet->GetSize() < header.GetSerializedSize())
            {
                ++m_droppedPackets;
                continue;
            }

            packet->PeekHeader(header);
            const uint64_t sequence = header.GetSequence();
            const uint32_t payloadSize = header.GetPayloadSize();

            if (sequence < m_nextExpected)
            {
                ++m_droppedPackets;
                continue;
            }

            if (sequence > m_nextExpected)
            {
                ++m_reorderedPackets;
            }

            m_buffer.emplace(sequence, payloadSize);

            while (true)
            {
                auto it = m_buffer.find(m_nextExpected);
                if (it == m_buffer.end())
                {
                    break;
                }

                m_deliveredBytes += it->second;
                ++m_deliveredPackets;
                m_buffer.erase(it);
                ++m_nextExpected;
            }
        }
    }

    std::vector<Ptr<Socket>> m_sockets;
    std::map<uint64_t, uint32_t> m_buffer;
    uint64_t m_nextExpected = 0;
    uint64_t m_receivedPackets = 0;
    uint64_t m_deliveredPackets = 0;
    uint64_t m_droppedPackets = 0;
    uint64_t m_reorderedPackets = 0;
    uint64_t m_deliveredBytes = 0;
};

class BondingSender : public Application
{
  public:
    void Configure(const std::vector<Ptr<Socket>>& sockets,
                   const std::vector<uint32_t>& packetRates,
                   uint32_t payloadBytes,
                   Time stopTime)
    {
        m_sockets = sockets;
        m_packetRates = packetRates;
        m_payloadBytes = payloadBytes;
        m_stopTime = stopTime;
    }

  private:
    void StartApplication() override
    {
        m_sequence = 0;
        m_linkCursor = 0;
        ScheduleNext();
    }

    void StopApplication() override
    {
        if (m_event.IsPending())
        {
            Simulator::Cancel(m_event);
        }
    }

    uint32_t SelectLink()
    {
        const uint32_t totalWeight =
            std::accumulate(m_packetRates.begin(), m_packetRates.end(), 0u);
        if (totalWeight == 0)
        {
            return 0;
        }

        uint32_t slot = m_linkCursor % totalWeight;
        ++m_linkCursor;

        for (uint32_t i = 0; i < m_packetRates.size(); ++i)
        {
            if (slot < m_packetRates[i])
            {
                return i;
            }
            slot -= m_packetRates[i];
        }
        return static_cast<uint32_t>(m_sockets.size() - 1);
    }

    void SendOne()
    {
        if (Simulator::Now() >= m_stopTime)
        {
            return;
        }

        const uint32_t link = SelectLink();
        Ptr<Packet> packet = Create<Packet>(m_payloadBytes);

        BondingHeader header;
        header.Set(m_sequence++, m_payloadBytes);
        packet->AddHeader(header);

        m_sockets[link]->Send(packet);
        ScheduleNext();
    }

    void ScheduleNext()
    {
        if (Simulator::Now() >= m_stopTime)
        {
            return;
        }

        m_event = Simulator::Schedule(
            MilliSeconds(1),
            &BondingSender::SendOne,
            this);
    }

    std::vector<Ptr<Socket>> m_sockets;
    std::vector<uint32_t> m_packetRates;
    uint32_t m_payloadBytes = 1200;
    Time m_stopTime = Seconds(30);
    EventId m_event;
    uint64_t m_sequence = 0;
    uint32_t m_linkCursor = 0;
};

} // namespace

int main(int argc, char* argv[])
{
    uint32_t packetBytes = 1200;
    double simulationSeconds = 30.0;
    uint32_t seed = 42;
    bool bonding = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetBytes", "Application payload bytes", packetBytes);
    cmd.AddValue("simulationSeconds", "Simulation duration", simulationSeconds);
    cmd.AddValue("seed", "RNG seed", seed);
    cmd.AddValue("bonding", "Enable multi-link striping; false uses link 1 only", bonding);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(1);

    NodeContainer sender;
    NodeContainer receiverNode;
    sender.Create(1);
    receiverNode.Create(1);

    const std::vector<std::string> dataRates = {"30Mbps", "20Mbps", "15Mbps"};
    const std::vector<std::string> delays = {"20ms", "35ms", "50ms"};
    const std::vector<Ipv4Address> receiverAddresses = {
        Ipv4Address("10.1.1.2"),
        Ipv4Address("10.1.2.2"),
        Ipv4Address("10.1.3.2")};

    InternetStackHelper internet;
    internet.Install(sender);
    internet.Install(receiverNode);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    for (uint32_t i = 0; i < 3; ++i)
    {
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(dataRates[i]));
        p2p.SetChannelAttribute("Delay", StringValue(delays[i]));

        NetDeviceContainer link = p2p.Install(sender.Get(0), receiverNode.Get(0));

        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.push_back(address.Assign(link));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    const uint16_t basePort = 5000;
    std::vector<Ptr<Socket>> senderSockets;

    auto receiver = CreateObject<BondingReceiver>();
    receiverNode.Get(0)->AddApplication(receiver);
    receiver->SetStartTime(Seconds(0.0));
    receiver->SetStopTime(Seconds(simulationSeconds));

    for (uint32_t i = 0; i < 3; ++i)
    {
        Ptr<Socket> rx =
            Socket::CreateSocket(receiverNode.Get(0), UdpSocketFactory::GetTypeId());
        rx->Bind(InetSocketAddress(Ipv4Address::GetAny(), basePort + i));
        receiver->AddSocket(rx);

        Ptr<Socket> tx =
            Socket::CreateSocket(sender.Get(0), UdpSocketFactory::GetTypeId());
        tx->Connect(InetSocketAddress(receiverAddresses[i], basePort + i));
        senderSockets.push_back(tx);
    }

    std::vector<uint32_t> weights = bonding
                                        ? std::vector<uint32_t>{6, 4, 3}
                                        : std::vector<uint32_t>{1, 0, 0};

    auto app = CreateObject<BondingSender>();
    app->Configure(senderSockets,
                   weights,
                   packetBytes,
                   Seconds(simulationSeconds));
    sender.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(0.1));
    app->SetStopTime(Seconds(simulationSeconds));

    Simulator::Stop(Seconds(simulationSeconds));

    std::cout << "\n====================================================\n";
    std::cout << " Railway Multi-Connectivity Gateway V12\n";
    std::cout << " Packet-Level Striping + Reordering Prototype\n";
    std::cout << "====================================================\n\n";
    std::cout << "Link 1: 30 Mbps, 20 ms\n";
    std::cout << "Link 2: 20 Mbps, 35 ms\n";
    std::cout << "Link 3: 15 Mbps, 50 ms\n";
    std::cout << "Bonding: " << (bonding ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "Packet payload: " << packetBytes << " bytes\n";
    std::cout << "Simulation: " << simulationSeconds << " seconds\n\n";

    Simulator::Run();

    const double elapsed = std::max(0.001, simulationSeconds - 0.1);
    const double goodputMbps =
        static_cast<double>(receiver->GetDeliveredBytes()) * 8.0 /
        elapsed / 1000000.0;

    std::cout << "----------- V12 RESULTS -----------\n";
    std::cout << "Received packets : " << receiver->GetReceivedPackets() << "\n";
    std::cout << "Delivered packets: " << receiver->GetDeliveredPackets() << "\n";
    std::cout << "Delivered bytes  : " << receiver->GetDeliveredBytes() << "\n";
    std::cout << "Dropped packets  : " << receiver->GetDroppedPackets() << "\n";
    std::cout << "Reordered packets: " << receiver->GetReorderedPackets() << "\n";
    std::cout << "Effective goodput: " << std::fixed << std::setprecision(3)
              << goodputMbps << " Mbps\n";
    std::cout << "------------------------------------\n\n";

    Simulator::Destroy();
    return 0;
}
