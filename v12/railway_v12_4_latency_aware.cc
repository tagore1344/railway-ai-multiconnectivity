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

class BondingHeader : public Header
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("RailwayBondingHeaderV124")
            .SetParent<Header>()
            .AddConstructor<BondingHeader>();
        return tid;
    }

    TypeId GetInstanceTypeId() const override { return GetTypeId(); }

    void Set(uint64_t seq, uint32_t bytes)
    {
        m_seq = seq;
        m_bytes = bytes;
    }

    uint64_t Seq() const { return m_seq; }
    uint32_t Bytes() const { return m_bytes; }

    uint32_t GetSerializedSize() const override { return 12; }

    void Serialize(Buffer::Iterator it) const override
    {
        it.WriteHtonU64(m_seq);
        it.WriteHtonU32(m_bytes);
    }

    uint32_t Deserialize(Buffer::Iterator it) override
    {
        m_seq = it.ReadNtohU64();
        m_bytes = it.ReadNtohU32();
        return GetSerializedSize();
    }

    void Print(std::ostream& os) const override
    {
        os << "seq=" << m_seq << " bytes=" << m_bytes;
    }

  private:
    uint64_t m_seq = 0;
    uint32_t m_bytes = 0;
};

class Receiver : public Application
{
  public:
    void AddSocket(Ptr<Socket> socket)
    {
        m_sockets.push_back(socket);
        socket->SetRecvCallback(MakeCallback(&Receiver::Read, this));
    }

    uint64_t Rx() const { return m_rx; }
    uint64_t Unique() const { return m_unique; }
    uint64_t InOrderBytes() const { return m_inOrderBytes; }
    uint64_t UniqueBytes() const { return m_uniqueBytes; }
    uint64_t Reordered() const { return m_reordered; }
    uint64_t GapTimeouts() const { return m_gapTimeouts; }
    uint64_t DeclaredLost() const { return m_lost; }
    uint64_t MaxBuffer() const { return m_maxBuffer; }
    uint64_t Late() const { return m_late; }

  private:
    static constexpr uint32_t kTimeoutMs = 80;

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

    void ArmGap(uint64_t expected)
    {
        if (m_gapEvent.IsPending())
        {
            Simulator::Cancel(m_gapEvent);
        }

        m_gapEvent = Simulator::Schedule(
            MilliSeconds(kTimeoutMs),
            &Receiver::Expire,
            this,
            expected);
    }

    void Expire(uint64_t expected)
    {
        if (expected != m_nextExpected || m_buffer.empty())
        {
            return;
        }

        auto first = m_buffer.begin();
        if (first->first > m_nextExpected)
        {
            m_lost += first->first - m_nextExpected;
            ++m_gapTimeouts;
            m_nextExpected = first->first;
        }

        Drain();
    }

    void Drain()
    {
        while (true)
        {
            auto it = m_buffer.find(m_nextExpected);
            if (it == m_buffer.end())
            {
                if (!m_buffer.empty())
                {
                    ArmGap(m_nextExpected);
                }
                return;
            }

            m_inOrderBytes += it->second;
            m_buffer.erase(it);
            ++m_nextExpected;
        }
    }

    void Read(Ptr<Socket> socket)
    {
        while (Ptr<Packet> packet = socket->Recv())
        {
            ++m_rx;

            BondingHeader header;
            if (packet->GetSize() < header.GetSerializedSize())
            {
                ++m_late;
                continue;
            }

            packet->PeekHeader(header);
            const uint64_t sequence = header.Seq();

            if (m_seen.find(sequence) != m_seen.end())
            {
                ++m_late;
                continue;
            }

            m_seen.emplace(sequence, true);
            ++m_unique;
            m_uniqueBytes += header.Bytes();

            if (sequence < m_nextExpected)
            {
                ++m_late;
                continue;
            }

            if (sequence > m_nextExpected)
            {
                ++m_reordered;
            }

            m_buffer.emplace(sequence, header.Bytes());
            m_maxBuffer = std::max<uint64_t>(m_maxBuffer, m_buffer.size());
            Drain();
        }
    }

    std::vector<Ptr<Socket>> m_sockets;
    std::map<uint64_t, uint32_t> m_buffer;
    std::map<uint64_t, bool> m_seen;
    uint64_t m_nextExpected = 0;
    uint64_t m_rx = 0;
    uint64_t m_unique = 0;
    uint64_t m_inOrderBytes = 0;
    uint64_t m_uniqueBytes = 0;
    uint64_t m_reordered = 0;
    uint64_t m_gapTimeouts = 0;
    uint64_t m_lost = 0;
    uint64_t m_maxBuffer = 0;
    uint64_t m_late = 0;
    EventId m_gapEvent;
};

class Sender : public Application
{
  public:
    void Configure(const std::vector<Ptr<Socket>>& sockets,
                   uint32_t payload,
                   uint32_t intervalUs,
                   bool bonding,
                   Time stop)
    {
        m_sockets = sockets;
        m_payload = payload;
        m_interval = MicroSeconds(intervalUs);
        m_bonding = bonding;
        m_stop = stop;
    }

  private:
    void StartApplication() override
    {
        Send();
    }

    void StopApplication() override
    {
        if (m_event.IsPending())
        {
            Simulator::Cancel(m_event);
        }
    }

    uint32_t PickLink()
    {
        if (!m_bonding)
        {
            return 0;
        }

        // Latency-aware/capacity-aware pattern. Link 1 has the lowest delay
        // and highest capacity, while Links 2 and 3 remain active enough to
        // contribute aggregate capacity without the symmetric 6:4:3 pattern
        // used by V12.3.
        static const uint32_t pattern[] = {
            0, 0, 1, 0, 2, 1, 0, 1, 2, 0, 1, 0, 2,
        };

        return pattern[m_cursor++ % 13];
    }

    void Send()
    {
        if (Simulator::Now() >= m_stop)
        {
            return;
        }

        const uint32_t link = PickLink();

        Ptr<Packet> packet = Create<Packet>(m_payload);
        BondingHeader header;
        header.Set(m_sequence++, m_payload);
        packet->AddHeader(header);

        m_sockets.at(link)->Send(packet);

        m_event = Simulator::Schedule(
            m_interval,
            &Sender::Send,
            this);
    }

    std::vector<Ptr<Socket>> m_sockets;
    uint32_t m_payload = 1200;
    uint32_t m_intervalUs = 150;
    uint32_t m_cursor = 0;
    bool m_bonding = true;
    Time m_interval = MicroSeconds(150);
    Time m_stop = Seconds(30);
    EventId m_event;
    uint64_t m_sequence = 0;
};

} // namespace

int main(int argc, char* argv[])
{
    uint32_t payload = 1200;
    uint32_t intervalUs = 150;
    double seconds = 30.0;
    bool bonding = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payload", "Payload bytes", payload);
    cmd.AddValue("intervalUs", "Inter-packet interval", intervalUs);
    cmd.AddValue("simulationSeconds", "Simulation duration", seconds);
    cmd.AddValue("bonding", "Enable latency-aware bonding", bonding);
    cmd.Parse(argc, argv);

    NodeContainer txNode;
    NodeContainer rxNode;
    txNode.Create(1);
    rxNode.Create(1);

    InternetStackHelper internet;
    internet.Install(txNode);
    internet.Install(rxNode);

    const std::vector<std::string> rates = {"30Mbps", "20Mbps", "15Mbps"};
    const std::vector<std::string> delays = {"20ms", "35ms", "50ms"};

    std::vector<Ptr<Socket>> txSockets;

    auto rxApp = CreateObject<Receiver>();
    rxNode.Get(0)->AddApplication(rxApp);
    rxApp->SetStartTime(Seconds(0));
    rxApp->SetStopTime(Seconds(seconds));

    for (uint32_t i = 0; i < 3; ++i)
    {
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(rates[i]));
        p2p.SetChannelAttribute("Delay", StringValue(delays[i]));

        auto devices = p2p.Install(txNode.Get(0), rxNode.Get(0));

        Ipv4AddressHelper address;
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(
            subnet.str().c_str(),
            "255.255.255.0");

        auto iface = address.Assign(devices);

        Ptr<Socket> receiverSocket =
            Socket::CreateSocket(
                rxNode.Get(0),
                UdpSocketFactory::GetTypeId());

        receiverSocket->Bind(
            InetSocketAddress(
                Ipv4Address::GetAny(),
                5000 + i));

        rxApp->AddSocket(receiverSocket);

        Ptr<Socket> senderSocket =
            Socket::CreateSocket(
                txNode.Get(0),
                UdpSocketFactory::GetTypeId());

        senderSocket->Connect(
            InetSocketAddress(
                iface.GetAddress(1),
                5000 + i));

        txSockets.push_back(senderSocket);
    }

    auto senderApp = CreateObject<Sender>();
    senderApp->Configure(
        txSockets,
        payload,
        intervalUs,
        bonding,
        Seconds(seconds));

    txNode.Get(0)->AddApplication(senderApp);
    senderApp->SetStartTime(Seconds(0.1));
    senderApp->SetStopTime(Seconds(seconds));

    Simulator::Stop(Seconds(seconds));
    Simulator::Run();

    const double measured =
        std::max(0.001, seconds - 0.1);

    const double offeredMbps =
        static_cast<double>(payload) * 8.0 * 1000.0 /
        static_cast<double>(intervalUs);

    const double uniqueMbps =
        static_cast<double>(rxApp->UniqueBytes()) * 8.0 /
        measured / 1e6;

    const double inorderMbps =
        static_cast<double>(rxApp->InOrderBytes()) * 8.0 /
        measured / 1e6;

    std::cout << "\n====================================================\n";
    std::cout << " Railway Multi-Connectivity Gateway V12.4\n";
    std::cout << " Latency-Aware Packet Reordering Experiment\n";
    std::cout << "====================================================\n\n";
    std::cout << "Link 1: 30 Mbps, 20 ms\n";
    std::cout << "Link 2: 20 Mbps, 35 ms\n";
    std::cout << "Link 3: 15 Mbps, 50 ms\n";
    std::cout << "Bonding: " << (bonding ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "Payload: " << payload << " bytes\n";
    std::cout << "Inter-packet interval: " << intervalUs << " us\n";
    std::cout << "Offered application rate: " << offeredMbps << " Mbps\n";
    std::cout << "Reorder timeout: 80 ms\n\n";

    std::cout << "----------- V12.4 RESULTS -----------\n";
    std::cout << "Received packets       : " << rxApp->Rx() << "\n";
    std::cout << "Unique packets         : " << rxApp->Unique() << "\n";
    std::cout << "Unique received bytes  : " << rxApp->UniqueBytes() << "\n";
    std::cout << "In-order bytes         : " << rxApp->InOrderBytes() << "\n";
    std::cout << "Reordered packets      : " << rxApp->Reordered() << "\n";
    std::cout << "Gap timeout events     : " << rxApp->GapTimeouts() << "\n";
    std::cout << "Declared lost packets  : " << rxApp->DeclaredLost() << "\n";
    std::cout << "Max reorder buffer     : " << rxApp->MaxBuffer() << " packets\n";
    std::cout << "Late/duplicate         : " << rxApp->Late() << "\n";
    std::cout << "Unique received goodput: " << uniqueMbps << " Mbps\n";
    std::cout << "In-order goodput       : " << inorderMbps << " Mbps\n";
    std::cout << "-------------------------------------\n\n";

    Simulator::Destroy();
    return 0;
}
