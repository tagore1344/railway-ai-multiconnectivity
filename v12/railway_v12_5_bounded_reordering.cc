#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

namespace
{

struct LinkState
{
    uint32_t index;
    uint32_t capacityMbps;
    uint32_t delayMs;
    uint64_t packetsSent = 0;
    Time nextAvailable = Seconds(0);
};

class BondingHeader : public Header
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("RailwayBondingHeaderV125")
            .SetParent<Header>()
            .AddConstructor<BondingHeader>();
        return tid;
    }

    TypeId GetInstanceTypeId() const override { return GetTypeId(); }

    void Set(uint64_t sequence, uint32_t bytes)
    {
        m_sequence = sequence;
        m_bytes = bytes;
    }

    uint64_t Sequence() const { return m_sequence; }
    uint32_t Bytes() const { return m_bytes; }

    uint32_t GetSerializedSize() const override { return 12; }

    void Serialize(Buffer::Iterator it) const override
    {
        it.WriteHtonU64(m_sequence);
        it.WriteHtonU32(m_bytes);
    }

    uint32_t Deserialize(Buffer::Iterator it) override
    {
        m_sequence = it.ReadNtohU64();
        m_bytes = it.ReadNtohU32();
        return GetSerializedSize();
    }

    void Print(std::ostream& os) const override
    {
        os << "seq=" << m_sequence << " bytes=" << m_bytes;
    }

  private:
    uint64_t m_sequence = 0;
    uint32_t m_bytes = 0;
};

class Receiver : public Application
{
  public:
    void AddSocket(Ptr<Socket> socket)
    {
        m_sockets.push_back(socket);
        socket->SetRecvCallback(MakeCallback(&Receiver::HandleRead, this));
    }

    uint64_t ReceivedPackets() const { return m_receivedPackets; }
    uint64_t UniquePackets() const { return m_uniquePackets; }
    uint64_t UniqueBytes() const { return m_uniqueBytes; }
    uint64_t InOrderBytes() const { return m_inOrderBytes; }
    uint64_t ReorderedPackets() const { return m_reorderedPackets; }
    uint64_t LatePackets() const { return m_latePackets; }
    uint64_t GapTimeouts() const { return m_gapTimeouts; }
    uint64_t DeclaredLost() const { return m_declaredLost; }
    uint64_t MaxBuffer() const { return m_maxBuffer; }

  private:
    static constexpr uint32_t kGapTimeoutMs = 70;
    static constexpr uint64_t kMaxBufferedPackets = 6000;

    void StartApplication() override {}

    void StopApplication() override
    {
        if (m_gapEvent.IsPending())
        {
            Simulator::Cancel(m_gapEvent);
        }
        for (auto socket : m_sockets)
        {
            if (socket)
            {
                socket->Close();
            }
        }
    }

    void ArmGapTimer(uint64_t expected)
    {
        if (m_gapEvent.IsPending())
        {
            Simulator::Cancel(m_gapEvent);
        }
        m_gapEvent = Simulator::Schedule(
            MilliSeconds(kGapTimeoutMs),
            &Receiver::ExpireGap,
            this,
            expected);
    }

    void ExpireGap(uint64_t expected)
    {
        if (expected != m_nextExpected || m_reorderBuffer.empty())
        {
            return;
        }

        auto first = m_reorderBuffer.begin();
        if (first->first > m_nextExpected)
        {
            m_declaredLost += first->first - m_nextExpected;
            ++m_gapTimeouts;
            m_nextExpected = first->first;
        }
        Drain();
    }

    void Drain()
    {
        while (true)
        {
            auto it = m_reorderBuffer.find(m_nextExpected);
            if (it == m_reorderBuffer.end())
            {
                if (!m_reorderBuffer.empty())
                {
                    ArmGapTimer(m_nextExpected);
                }
                return;
            }

            m_inOrderBytes += it->second;
            m_reorderBuffer.erase(it);
            ++m_nextExpected;
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
                ++m_latePackets;
                continue;
            }

            packet->PeekHeader(header);
            const uint64_t sequence = header.Sequence();
            const uint32_t bytes = header.Bytes();

            if (m_seen.find(sequence) != m_seen.end())
            {
                ++m_latePackets;
                continue;
            }

            m_seen.emplace(sequence, true);
            ++m_uniquePackets;
            m_uniqueBytes += bytes;

            if (sequence < m_nextExpected)
            {
                ++m_latePackets;
                continue;
            }

            if (sequence > m_nextExpected)
            {
                ++m_reorderedPackets;
            }

            m_reorderBuffer.emplace(sequence, bytes);
            m_maxBuffer = std::max<uint64_t>(m_maxBuffer, m_reorderBuffer.size());

            // Hard cap: release the oldest gap when the buffer becomes too
            // large so that pathological delay differences cannot grow the
            // receiver indefinitely.
            if (m_reorderBuffer.size() > kMaxBufferedPackets)
            {
                auto first = m_reorderBuffer.begin();
                if (first->first > m_nextExpected)
                {
                    m_declaredLost += first->first - m_nextExpected;
                    ++m_gapTimeouts;
                    m_nextExpected = first->first;
                }
            }

            Drain();
        }
    }

    std::vector<Ptr<Socket>> m_sockets;
    std::map<uint64_t, uint32_t> m_reorderBuffer;
    std::map<uint64_t, bool> m_seen;
    uint64_t m_nextExpected = 0;
    uint64_t m_receivedPackets = 0;
    uint64_t m_uniquePackets = 0;
    uint64_t m_uniqueBytes = 0;
    uint64_t m_inOrderBytes = 0;
    uint64_t m_reorderedPackets = 0;
    uint64_t m_latePackets = 0;
    uint64_t m_gapTimeouts = 0;
    uint64_t m_declaredLost = 0;
    uint64_t m_maxBuffer = 0;
    EventId m_gapEvent;
};

class LatencyAwareSender : public Application
{
  public:
    void Configure(const std::vector<Ptr<Socket>>& sockets,
                   uint32_t payloadBytes,
                   uint32_t intervalUs,
                   bool bonding,
                   Time stopTime)
    {
        m_sockets = sockets;
        m_payloadBytes = payloadBytes;
        m_interval = MicroSeconds(intervalUs);
        m_bonding = bonding;
        m_stopTime = stopTime;

        m_links = {
            {0, 30, 20},
            {1, 20, 35},
            {2, 15, 50},
        };
    }

  private:
    void StartApplication() override
    {
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
        if (!m_bonding)
        {
            return 0;
        }

        const Time now = Simulator::Now();
        const double packetBits = static_cast<double>(m_payloadBytes) * 8.0;

        double bestScore = 1e30;
        uint32_t best = 0;

        for (auto& link : m_links)
        {
            // Estimate serialization completion plus propagation delay for a
            // packet placed on this path now. A small utilization penalty
            // prevents the lower-delay link from monopolizing traffic.
            const double serviceSeconds =
                packetBits / (static_cast<double>(link.capacityMbps) * 1e6);
            const Time serialization = Seconds(serviceSeconds);
            const Time predictedArrival =
                std::max(now, link.nextAvailable) +
                serialization + MilliSeconds(link.delayMs);

            const double finishDelta =
                (predictedArrival - now).GetSeconds();

            const double utilizationPenalty =
                0.002 * static_cast<double>(link.packetsSent % 1000);

            const double score = finishDelta + utilizationPenalty;

            if (score < bestScore)
            {
                bestScore = score;
                best = link.index;
            }
        }

        return best;
    }

    void SendOne()
    {
        if (Simulator::Now() >= m_stopTime)
        {
            return;
        }

        const uint32_t linkIndex = SelectLink();

        Ptr<Packet> packet = Create<Packet>(m_payloadBytes);
        BondingHeader header;
        header.Set(m_sequence++, m_payloadBytes);
        packet->AddHeader(header);

        m_sockets.at(linkIndex)->Send(packet);

        const double serviceSeconds =
            (static_cast<double>(m_payloadBytes) * 8.0) /
            (static_cast<double>(m_links[linkIndex].capacityMbps) * 1e6);

        m_links[linkIndex].nextAvailable =
            std::max(Simulator::Now(), m_links[linkIndex].nextAvailable) +
            Seconds(serviceSeconds);
        ++m_links[linkIndex].packetsSent;

        m_event = Simulator::Schedule(
            m_interval,
            &LatencyAwareSender::SendOne,
            this);
    }

    std::vector<Ptr<Socket>> m_sockets;
    std::vector<LinkState> m_links;
    uint32_t m_payloadBytes = 1200;
    uint32_t m_intervalUs = 150;
    bool m_bonding = true;
    Time m_interval = MicroSeconds(150);
    Time m_stopTime = Seconds(30);
    EventId m_event;
    uint64_t m_sequence = 0;
};

} // namespace

int main(int argc, char* argv[])
{
    uint32_t payloadBytes = 1200;
    uint32_t intervalUs = 150;
    double simulationSeconds = 30.0;
    bool bonding = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payload", "Payload bytes", payloadBytes);
    cmd.AddValue("intervalUs", "Inter-packet interval in microseconds", intervalUs);
    cmd.AddValue("simulationSeconds", "Simulation duration", simulationSeconds);
    cmd.AddValue("bonding", "Enable latency-aware multi-link bonding", bonding);
    cmd.Parse(argc, argv);

    NodeContainer sender;
    NodeContainer receiver;
    sender.Create(1);
    receiver.Create(1);

    InternetStackHelper internet;
    internet.Install(sender);
    internet.Install(receiver);

    const std::vector<std::string> rates = {"30Mbps", "20Mbps", "15Mbps"};
    const std::vector<std::string> delays = {"20ms", "35ms", "50ms"};

    std::vector<Ptr<Socket>> txSockets;

    auto rxApp = CreateObject<Receiver>();
    receiver.Get(0)->AddApplication(rxApp);
    rxApp->SetStartTime(Seconds(0));
    rxApp->SetStopTime(Seconds(simulationSeconds));

    for (uint32_t i = 0; i < 3; ++i)
    {
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(rates[i]));
        p2p.SetChannelAttribute("Delay", StringValue(delays[i]));

        NetDeviceContainer devices =
            p2p.Install(sender.Get(0), receiver.Get(0));

        Ipv4AddressHelper address;
        std::ostringstream subnet;
        subnet << "10.2." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(devices);

        Ptr<Socket> rxSocket =
            Socket::CreateSocket(
                receiver.Get(0),
                UdpSocketFactory::GetTypeId());
        rxSocket->Bind(
            InetSocketAddress(
                Ipv4Address::GetAny(),
                6000 + i));
        rxApp->AddSocket(rxSocket);

        Ptr<Socket> txSocket =
            Socket::CreateSocket(
                sender.Get(0),
                UdpSocketFactory::GetTypeId());
        txSocket->Connect(
            InetSocketAddress(
                iface.GetAddress(1),
                6000 + i));
        txSockets.push_back(txSocket);
    }

    auto txApp = CreateObject<LatencyAwareSender>();
    txApp->Configure(
        txSockets,
        payloadBytes,
        intervalUs,
        bonding,
        Seconds(simulationSeconds));
    sender.Get(0)->AddApplication(txApp);
    txApp->SetStartTime(Seconds(0.1));
    txApp->SetStopTime(Seconds(simulationSeconds));

    Simulator::Stop(Seconds(simulationSeconds));
    Simulator::Run();

    const double measuredSeconds =
        std::max(0.001, simulationSeconds - 0.1);

    const double offeredMbps =
        static_cast<double>(payloadBytes) * 8.0 * 1000.0 /
        static_cast<double>(intervalUs);

    const double uniqueGoodput =
        static_cast<double>(rxApp->UniqueBytes()) * 8.0 /
        measuredSeconds / 1e6;

    const double inOrderGoodput =
        static_cast<double>(rxApp->InOrderBytes()) * 8.0 /
        measuredSeconds / 1e6;

    const double reorderRate =
        rxApp->ReceivedPackets() == 0
            ? 0.0
            : 100.0 * static_cast<double>(rxApp->ReorderedPackets()) /
                  static_cast<double>(rxApp->ReceivedPackets());

    std::cout << "\n====================================================\n";
    std::cout << " Railway Multi-Connectivity Gateway V12.5\n";
    std::cout << " Bounded Latency-Aware Packet Bonding\n";
    std::cout << "====================================================\n\n";
    std::cout << "Link 1: 30 Mbps, 20 ms\n";
    std::cout << "Link 2: 20 Mbps, 35 ms\n";
    std::cout << "Link 3: 15 Mbps, 50 ms\n";
    std::cout << "Bonding: " << (bonding ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "Payload: " << payloadBytes << " bytes\n";
    std::cout << "Inter-packet interval: " << intervalUs << " us\n";
    std::cout << "Offered application rate: " << offeredMbps << " Mbps\n";
    std::cout << "Gap timeout: 70 ms\n";
    std::cout << "Maximum reorder buffer: 6000 packets\n\n";

    std::cout << "----------- V12.5 RESULTS -----------\n";
    std::cout << "Received packets       : " << rxApp->ReceivedPackets() << "\n";
    std::cout << "Unique packets         : " << rxApp->UniquePackets() << "\n";
    std::cout << "Unique received bytes  : " << rxApp->UniqueBytes() << "\n";
    std::cout << "In-order bytes         : " << rxApp->InOrderBytes() << "\n";
    std::cout << "Reordered packets      : " << rxApp->ReorderedPackets() << "\n";
    std::cout << "Reorder rate           : " << reorderRate << "%\n";
    std::cout << "Late/duplicate         : " << rxApp->LatePackets() << "\n";
    std::cout << "Gap timeout events     : " << rxApp->GapTimeouts() << "\n";
    std::cout << "Declared lost packets  : " << rxApp->DeclaredLost() << "\n";
    std::cout << "Max reorder buffer     : " << rxApp->MaxBuffer() << " packets\n";
    std::cout << "Unique received goodput: " << uniqueGoodput << " Mbps\n";
    std::cout << "In-order goodput       : " << inOrderGoodput << " Mbps\n";
    std::cout << "-------------------------------------\n\n";

    Simulator::Destroy();
    return 0;
}
