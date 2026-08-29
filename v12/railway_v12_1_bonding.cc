#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

namespace
{

class BondingHeader : public Header
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("RailwayBondingHeader")
            .SetParent<Header>()
            .AddConstructor<BondingHeader>();
        return tid;
    }

    TypeId GetInstanceTypeId() const override { return GetTypeId(); }

    void Set(uint64_t sequence, uint32_t payloadBytes)
    {
        m_sequence = sequence;
        m_payloadBytes = payloadBytes;
    }

    uint64_t GetSequence() const { return m_sequence; }
    uint32_t GetPayloadBytes() const { return m_payloadBytes; }

    uint32_t GetSerializedSize() const override { return 12; }

    void Serialize(Buffer::Iterator it) const override
    {
        it.WriteHtonU64(m_sequence);
        it.WriteHtonU32(m_payloadBytes);
    }

    uint32_t Deserialize(Buffer::Iterator it) override
    {
        m_sequence = it.ReadNtohU64();
        m_payloadBytes = it.ReadNtohU32();
        return 12;
    }

    void Print(std::ostream& os) const override
    {
        os << "seq=" << m_sequence << " bytes=" << m_payloadBytes;
    }

  private:
    uint64_t m_sequence = 0;
    uint32_t m_payloadBytes = 0;
};

class BondingReceiver : public Application
{
  public:
    void AddSocket(Ptr<Socket> socket)
    {
        m_sockets.push_back(socket);
        socket->SetRecvCallback(MakeCallback(&BondingReceiver::HandleRead, this));
    }

    uint64_t ReceivedPackets() const { return m_receivedPackets; }
    uint64_t DeliveredPackets() const { return m_deliveredPackets; }
    uint64_t DeliveredBytes() const { return m_deliveredBytes; }
    uint64_t LateOrDuplicatePackets() const { return m_lateOrDuplicate; }
    uint64_t ReorderedPackets() const { return m_reorderedPackets; }

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
        while (Ptr<Packet> packet = socket->Recv())
        {
            ++m_receivedPackets;

            BondingHeader header;
            if (packet->GetSize() < header.GetSerializedSize())
            {
                ++m_lateOrDuplicate;
                continue;
            }

            packet->PeekHeader(header);
            const uint64_t sequence = header.GetSequence();
            const uint32_t payloadBytes = header.GetPayloadBytes();

            if (sequence < m_nextExpected)
            {
                ++m_lateOrDuplicate;
                continue;
            }

            if (sequence > m_nextExpected)
            {
                ++m_reorderedPackets;
            }

            m_reorderBuffer.emplace(sequence, payloadBytes);

            while (true)
            {
                auto it = m_reorderBuffer.find(m_nextExpected);
                if (it == m_reorderBuffer.end())
                {
                    break;
                }

                m_deliveredBytes += it->second;
                ++m_deliveredPackets;
                m_reorderBuffer.erase(it);
                ++m_nextExpected;
            }
        }
    }

    std::vector<Ptr<Socket>> m_sockets;
    std::map<uint64_t, uint32_t> m_reorderBuffer;
    uint64_t m_nextExpected = 0;
    uint64_t m_receivedPackets = 0;
    uint64_t m_deliveredPackets = 0;
    uint64_t m_deliveredBytes = 0;
    uint64_t m_lateOrDuplicate = 0;
    uint64_t m_reorderedPackets = 0;
};

class SaturatingBondingSender : public Application
{
  public:
    void Configure(const std::vector<Ptr<Socket>>& sockets,
                   const std::vector<uint32_t>& weights,
                   uint32_t payloadBytes,
                   uint32_t intervalUs,
                   Time stopTime)
    {
        m_sockets = sockets;
        m_weights = weights;
        m_payloadBytes = payloadBytes;
        m_interval = MicroSeconds(intervalUs);
        m_stopTime = stopTime;
    }

  private:
    void StartApplication() override
    {
        m_sequence = 0;
        m_weightCursor = 0;
        SendOne();
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
        const uint32_t total = std::accumulate(m_weights.begin(), m_weights.end(), 0u);
        if (total == 0 || m_sockets.empty())
        {
            return 0;
        }

        const uint32_t slot = m_weightCursor++ % total;
        uint32_t remaining = slot;

        for (uint32_t i = 0; i < m_weights.size(); ++i)
        {
            if (remaining < m_weights[i])
            {
                return i;
            }
            remaining -= m_weights[i];
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

        m_sockets.at(link)->Send(packet);

        m_event = Simulator::Schedule(m_interval,
                                      &SaturatingBondingSender::SendOne,
                                      this);
    }

    std::vector<Ptr<Socket>> m_sockets;
    std::vector<uint32_t> m_weights;
    uint32_t m_payloadBytes = 1200;
    Time m_interval = MicroSeconds(150);
    Time m_stopTime = Seconds(30.0);
    EventId m_event;
    uint64_t m_sequence = 0;
    uint32_t m_weightCursor = 0;
};

} // namespace

int main(int argc, char* argv[])
{
    uint32_t packetBytes = 1200;
    uint32_t intervalUs = 150;
    double simulationSeconds = 30.0;
    bool bonding = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetBytes", "Payload bytes per packet", packetBytes);
    cmd.AddValue("intervalUs", "Inter-packet interval in microseconds", intervalUs);
    cmd.AddValue("simulationSeconds", "Simulation duration", simulationSeconds);
    cmd.AddValue("bonding", "Enable three-link bonding", bonding);
    cmd.Parse(argc, argv);

    NodeContainer sender;
    NodeContainer receiverNode;
    sender.Create(1);
    receiverNode.Create(1);

    InternetStackHelper internet;
    internet.Install(sender);
    internet.Install(receiverNode);

    const std::vector<std::string> rates = {"30Mbps", "20Mbps", "15Mbps"};
    const std::vector<std::string> delays = {"20ms", "35ms", "50ms"};
    const std::vector<Ipv4Address> dst = {
        Ipv4Address("10.1.1.2"),
        Ipv4Address("10.1.2.2"),
        Ipv4Address("10.1.3.2")};

    std::vector<Ptr<Socket>> txSockets;
    auto rxApp = CreateObject<BondingReceiver>();
    receiverNode.Get(0)->AddApplication(rxApp);
    rxApp->SetStartTime(Seconds(0.0));
    rxApp->SetStopTime(Seconds(simulationSeconds));

    for (uint32_t i = 0; i < 3; ++i)
    {
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(rates[i]));
        p2p.SetChannelAttribute("Delay", StringValue(delays[i]));

        NetDeviceContainer dev = p2p.Install(sender.Get(0), receiverNode.Get(0));

        Ipv4AddressHelper address;
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(dev);

        Ptr<Socket> rx = Socket::CreateSocket(receiverNode.Get(0), UdpSocketFactory::GetTypeId());
        rx->Bind(InetSocketAddress(Ipv4Address::GetAny(), 5000 + i));
        rxApp->AddSocket(rx);

        Ptr<Socket> tx = Socket::CreateSocket(sender.Get(0), UdpSocketFactory::GetTypeId());
        tx->Connect(InetSocketAddress(iface.GetAddress(1), 5000 + i));
        txSockets.push_back(tx);
    }

    const std::vector<uint32_t> weights =
        bonding ? std::vector<uint32_t>{6, 4, 3}
                : std::vector<uint32_t>{1, 0, 0};

    auto senderApp = CreateObject<SaturatingBondingSender>();
    senderApp->Configure(txSockets,
                         weights,
                         packetBytes,
                         intervalUs,
                         Seconds(simulationSeconds));
    sender.Get(0)->AddApplication(senderApp);
    senderApp->SetStartTime(Seconds(0.1));
    senderApp->SetStopTime(Seconds(simulationSeconds));

    Simulator::Stop(Seconds(simulationSeconds));
    Simulator::Run();

    const double measuredSeconds = std::max(0.001, simulationSeconds - 0.1);
    const double goodputMbps =
        static_cast<double>(rxApp->DeliveredBytes()) * 8.0 /
        measuredSeconds / 1000000.0;

    const double offeredMbps =
        static_cast<double>(packetBytes) * 8.0 /
        static_cast<double>(intervalUs) / 1000.0;

    std::cout << "\n====================================================\n";
    std::cout << " Railway Multi-Connectivity Gateway V12.1\n";
    std::cout << " Saturating Packet-Level Bonding Experiment\n";
    std::cout << "====================================================\n\n";
    std::cout << "Link 1: 30 Mbps, 20 ms\n";
    std::cout << "Link 2: 20 Mbps, 35 ms\n";
    std::cout << "Link 3: 15 Mbps, 50 ms\n";
    std::cout << "Bonding: " << (bonding ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "Payload: " << packetBytes << " bytes\n";
    std::cout << "Inter-packet interval: " << intervalUs << " us\n";
    std::cout << "Offered application rate: " << offeredMbps << " Mbps\n";
    std::cout << "Simulation: " << simulationSeconds << " seconds\n\n";

    std::cout << "----------- V12.1 RESULTS -----------\n";
    std::cout << "Received packets    : " << rxApp->ReceivedPackets() << "\n";
    std::cout << "Delivered packets   : " << rxApp->DeliveredPackets() << "\n";
    std::cout << "Delivered bytes     : " << rxApp->DeliveredBytes() << "\n";
    std::cout << "Late/duplicate      : " << rxApp->LateOrDuplicatePackets() << "\n";
    std::cout << "Reordered packets   : " << rxApp->ReorderedPackets() << "\n";
    std::cout << "Effective goodput   : " << goodputMbps << " Mbps\n";
    std::cout << "-------------------------------------\n\n";

    Simulator::Destroy();
    return 0;
}
